#include "mock_ovms.hpp"

MetricStore         g_metrics;
uint32_t            g_tick_count = 0;
StandardMetricsType StandardMetrics;
OvmsConfig          MyConfig;
OvmsCommandApp      MyCommandApp;
OvmsVehicleFactory  MyVehicleFactory;
