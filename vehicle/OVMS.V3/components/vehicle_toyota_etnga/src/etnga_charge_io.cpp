/*
;    Project:       Open Vehicle Monitor System
;    e-TNGA async charge file-I/O worker.
;
; Decouples charge-report file writes (CSV flush, HTML report, session-end unlink, prune)
; from the Events task: producers enqueue heap-owned jobs; one low-priority worker task
; performs the actual SD I/O, so a stalled SD can never block charge polling.
; See docs/superpowers/specs/2026-06-20-etnga-charge-async-io-design.md
*/

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include "ovms_log.h"
#include "vehicle_toyota_etnga.h"

static const char* TAG = "v-etnga";

static const int CHARGE_REPORT_MAX = 50;   // retain at most this many reports (moved from etnga_charge_report.cpp)

// Retain only the newest CHARGE_REPORT_MAX sessions; delete both .html and .csv for each
// pruned stem. Also removes orphan .csv files whose session never produced an .html.
// (Relocated verbatim from etnga_charge_report.cpp so it runs on the worker, not Events.)
static void PruneChargeReports(const std::string& dir)
{
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> stems;        // .html stems (a complete report)
    std::vector<std::string> csv_stems;    // .csv stems (may be orphaned)
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        std::string n = e->d_name;
        if (n.size() > 5 && n.compare(n.size()-5, 5, ".html") == 0)
            stems.push_back(n.substr(0, n.size()-5));
        else if (n.size() > 4 && n.compare(n.size()-4, 4, ".csv") == 0)
            csv_stems.push_back(n.substr(0, n.size()-4));
    }
    closedir(d);

    for (size_t i = 0; i < csv_stems.size(); i++) {
        bool has_html = false;
        for (size_t j = 0; j < stems.size(); j++)
            if (stems[j] == csv_stems[i]) { has_html = true; break; }
        if (!has_html)
            unlink((dir + "/" + csv_stems[i] + ".csv").c_str());
    }

    if ((int)stems.size() <= CHARGE_REPORT_MAX) return;
    std::sort(stems.begin(), stems.end());
    int del = (int)stems.size() - CHARGE_REPORT_MAX;
    for (int i = 0; i < del; i++) {
        unlink((dir + "/" + stems[i] + ".html").c_str());
        unlink((dir + "/" + stems[i] + ".csv").c_str());
        ESP_LOGD(TAG, "Charge report pruned: %s", stems[i].c_str());
    }
}

void OvmsVehicleToyotaETNGA::ChargeIoTaskEntry(void* arg)
{
    static_cast<OvmsVehicleToyotaETNGA*>(arg)->ChargeIoTask();
}

void OvmsVehicleToyotaETNGA::ChargeIoTask()
{
    etnga_io_job* job = NULL;
    for (;;) {
        if (xQueueReceive(m_io_queue, &job, portMAX_DELAY) != pdTRUE) continue;
        if (job->op == etnga_io_job::STOP) { delete job; break; }
        if (job->op == etnga_io_job::UNLINK) {
            remove(job->path.c_str());
        } else {   // WRITE_APPEND / WRITE_TRUNCATE
            FILE* f = fopen(job->path.c_str(), job->op == etnga_io_job::WRITE_APPEND ? "a" : "w");
            if (f) {
                size_t n = fwrite(job->data.data(), 1, job->data.size(), f);
                fclose(f);
                if (n != job->data.size())
                    ESP_LOGW(TAG, "charge-io: short write %s (%u/%u)", job->path.c_str(),
                        (unsigned)n, (unsigned)job->data.size());
                else
                    ESP_LOGD(TAG, "charge-io: wrote %u B to %s", (unsigned)n, job->path.c_str());
                if (!job->prune_dir.empty()) PruneChargeReports(job->prune_dir);
            } else {
                ESP_LOGW(TAG, "charge-io: cannot open %s", job->path.c_str());
            }
        }
        delete job;
    }
    m_io_task = NULL;
    xSemaphoreGive(m_io_stopped);
    vTaskDelete(NULL);
}

void OvmsVehicleToyotaETNGA::StartChargeIoTask()
{
    if (m_io_task) return;
    if (!m_io_queue)   m_io_queue   = xQueueCreate(8, sizeof(etnga_io_job*));
    if (!m_io_stopped) m_io_stopped = xSemaphoreCreateBinary();
    if (m_io_queue && m_io_stopped)
        xTaskCreatePinnedToCore(ChargeIoTaskEntry, "etnga_io", 4096, this, 2, &m_io_task, CORE(1));
    else
        ESP_LOGE(TAG, "charge-io: queue/semaphore alloc failed, worker not started");
}

bool OvmsVehicleToyotaETNGA::ChargeIoEnqueue(etnga_io_job* job)
{
    StartChargeIoTask();
    if (!m_io_queue || !m_io_task || xQueueSend(m_io_queue, &job, 0) != pdTRUE) {
        m_io_dropcnt++;
        ESP_LOGW(TAG, "charge-io: queue full or worker not running, dropped write to %s (dropcnt=%u)",
            job->path.c_str(), m_io_dropcnt);
        delete job;
        return false;
    }
    return true;
}

void OvmsVehicleToyotaETNGA::StopChargeIoTask()
{
    if (!m_io_task) return;
    etnga_io_job* job = new etnga_io_job;
    job->op = etnga_io_job::STOP;
    if (xQueueSend(m_io_queue, &job, pdMS_TO_TICKS(100)) != pdTRUE) { delete job; return; }
    // Bounded residual: if the worker is wedged in an SD write past the 2s join (in-flight write
    // + stalled SD during a no-reboot vehicle switch — near-impossible in practice, as the queue
    // is empty between charges), it could outlive this object. We accept that over a vTaskDelete
    // here, which risks a worse FATFS-lock wedge if the task is mid-fwrite.
    // Wait for the worker to drain and exit, but never block module shutdown forever.
    if (m_io_stopped) xSemaphoreTake(m_io_stopped, pdMS_TO_TICKS(2000));
    // m_io_queue / m_io_stopped are intentionally NOT deleted here: per the bounded residual
    // above the worker may still reference them past the join, so freeing them would risk a UAF.
    // The leak (one 8-deep queue + a binary semaphore) is bounded — the vehicle object is a
    // rarely-swapped singleton — and matches the OVMS pattern of not freeing task primitives.
}
