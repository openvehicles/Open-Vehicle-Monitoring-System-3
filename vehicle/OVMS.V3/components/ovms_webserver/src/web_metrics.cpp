/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th March 2026
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2026       Carsten Schmiemann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_webserver.h"

/**
 * HandleMetrics: display all metrics with live updates
 */
void OvmsWebServer::HandleMetrics(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<style>\n"
      "#metrics-table { font-size: 13px; }\n"
      "#metrics-table td { font-family: \"Menlo\", \"Consolas\", \"Liberation Mono\", monospace; }\n"
      "#metrics-table .value-cell { word-break: break-all; }\n"
      "#metrics-table .value-cell.updated { background-color: rgba(0, 212, 255, 0.25); transition: background-color 0s; }\n"
      ".night #metrics-table .value-cell.updated { background-color: rgba(0, 212, 255, 0.25); }\n"
      "#metrics-table .value-cell.flash { background-color: transparent; transition: background-color 0.8s ease-out; }\n"
      ".filter-buttons { margin-bottom: 10px; }\n"
      ".filter-buttons .btn { margin-right: 5px; margin-bottom: 5px; }\n"
      ".filter-buttons .btn.active { font-weight: bold; }\n"
      ".dataTables_filter { margin-bottom: 10px; }\n"
    "</style>\n"
    "<div class=\"panel panel-primary panel-single receiver\" id=\"metrics-panel\">\n"
      "<div class=\"panel-heading\">Metrics</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"filter-buttons\">\n"
          "<button type=\"button\" class=\"btn btn-default active\" data-filter=\"\">All</button>\n"
          "<button type=\"button\" class=\"btn btn-default\" data-filter=\"^v\\.\">Vehicle (v.)</button>\n"
          "<button type=\"button\" class=\"btn btn-default\" data-filter=\"^m\\.\">Module (m.)</button>\n"
          "<button type=\"button\" class=\"btn btn-default\" data-filter=\"^s\\.\">Server (s.)</button>\n"
          "<button type=\"button\" class=\"btn btn-default\" data-filter=\"^x\">Custom (x*)</button>\n"
        "</div>\n"
        "<table id=\"metrics-table\" class=\"table table-striped table-bordered table-condensed\">\n"
          "<thead>\n"
            "<tr>\n"
              "<th>Metric</th>\n"
              "<th>Value</th>\n"
              "<th>Unit</th>\n"
            "</tr>\n"
          "</thead>\n"
          "<tbody>\n"
          "</tbody>\n"
        "</table>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<script>\n"
    "(function(){\n"
    "\n"
      "var metricsTable;\n"
      "var currentFilter = '';\n"
    "\n"
      "function formatValue(val) {\n"
        "if (val === null || val === undefined) return '';\n"
        "if (Array.isArray(val)) {\n"
          "if (val.length === 0) return '[]';\n"
          "if (val.length <= 10) return '[' + val.join(', ') + ']';\n"
          "return '[' + val.slice(0, 10).join(', ') + ', ... (' + val.length + ' items)]';\n"
        "}\n"
        "if (typeof val === 'object') return JSON.stringify(val);\n"
        "return String(val);\n"
      "}\n"
    "\n"
      "function buildTable() {\n"
        "var rows = [];\n"
        "var names = Object.keys(metrics).sort();\n"
        "for (var i = 0; i < names.length; i++) {\n"
          "var name = names[i];\n"
          "var val = metrics[name];\n"
          "var unit = metrics_label[name] || '';\n"
          "rows.push([name, formatValue(val), unit]);\n"
        "}\n"
        "return rows;\n"
      "}\n"
    "\n"
      "function initTable() {\n"
        "var data = buildTable();\n"
        "metricsTable = $('#metrics-table').DataTable({\n"
          "data: data,\n"
          "columns: [\n"
            "{ title: 'Metric', width: '35%' },\n"
            "{ title: 'Value', width: '50%', className: 'value-cell' },\n"
            "{ title: 'Unit', width: '15%' }\n"
          "],\n"
          "order: [[0, 'asc']],\n"
          "paging: false,\n"
          "info: false,\n"
          "searching: true,\n"
          "autoWidth: false,\n"
          "responsive: true,\n"
          "language: {\n"
            "search: 'Search:',\n"
            "zeroRecords: 'No matching metrics found'\n"
          "}\n"
        "});\n"
        "$('#metrics-table').data('dataTable', metricsTable).addClass('has-dataTable');\n"
      "}\n"
      "function updateMetrics(update) {\n"
        "if (!metricsTable) return;\n"
        "var updatedRows = [];\n"
        "metricsTable.rows().every(function(rowIdx) {\n"
          "var data = this.data();\n"
          "var name = data[0];\n"
          "if (update[name] !== undefined) {\n"
            "var newVal = formatValue(metrics[name]);\n"
            "var newUnit = metrics_label[name] || '';\n"
            "if (data[1] !== newVal || data[2] !== newUnit) {\n"
              "data[1] = newVal;\n"
              "data[2] = newUnit;\n"
              "this.data(data);\n"
              "updatedRows.push(rowIdx);\n"
            "}\n"
          "}\n"
        "});\n"
        "// Check for new metrics\n"
        "var existingNames = {};\n"
        "metricsTable.rows().every(function() {\n"
          "existingNames[this.data()[0]] = true;\n"
        "});\n"
        "var newRows = [];\n"
        "for (var name in update) {\n"
          "if (!existingNames[name]) {\n"
            "var val = metrics[name];\n"
            "var unit = metrics_label[name] || '';\n"
            "newRows.push([name, formatValue(val), unit]);\n"
          "}\n"
        "}\n"
        "if (newRows.length > 0) {\n"
          "metricsTable.rows.add(newRows).draw(false);\n"
        "}\n"
        "// Flash updated rows\n"
        "if (updatedRows.length > 0) {\n"
          "metricsTable.rows(updatedRows).nodes().to$().find('.value-cell')\n"
            ".addClass('updated')\n"
            ".each(function() {\n"
              "var el = this;\n"
              "setTimeout(function() {\n"
                "$(el).addClass('flash').removeClass('updated');\n"
                "setTimeout(function() { $(el).removeClass('flash'); }, 500);\n"
              "}, 50);\n"
            "});\n"
        "}\n"
      "}\n"
      "function applyFilter(filter) {\n"
        "currentFilter = filter;\n"
        "if (metricsTable) {\n"
          "metricsTable.column(0).search(filter, true, false).draw();\n"
        "}\n"
      "}\n"
      "// Filter button handling\n"
      "$('.filter-buttons .btn').on('click', function() {\n"
        "var filter = $(this).data('filter');\n"
        "$('.filter-buttons .btn').removeClass('active');\n"
        "$(this).addClass('active');\n"
        "applyFilter(filter);\n"
      "});\n"
      "// Handle metrics updates\n"
      "$('#metrics-panel').on('msg:metrics', function(e, update) {\n"
        "updateMetrics(update);\n"
      "});\n"
      "// Initialize table\n"
      "if (window.DataTable) {\n"
        "initTable();\n"
      "} else {\n"
        "$.ajax({\n"
          "url: \"" URL_ASSETS_TABLES_JS "\",\n"
          "dataType: \"script\",\n"
          "cache: true,\n"
          "success: function() { initTable(); }\n"
        "});\n"
      "}\n"
    "})();\n"
    "</script>\n");

  PAGE_HOOK("body.post");
  c.done();
}
