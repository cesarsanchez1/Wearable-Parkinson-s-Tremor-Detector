#include <mbed.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <algorithm>  

#include <iostream>
#include <vector>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <thread>

#include "drivers/LCD_DISCO_F429ZI.h"


#define BACKGROUND 1
#define FOREGROUND 0
#define GRAPH_PADDING 5

#define CTRL_REG1 0x20
#define CTRL_REG1_CONFIG 0b01101111
#define CTRL_REG4 0x23
#define CTRL_REG4_CONFIG 0b00010000
#define SPI_FLAG 1
#define OUT_X_L 0x28
#define SAMPLES 128
#define FFT_SIZE (SAMPLES / 2)

#define SCALING_FACTOR (17.5f * 0.0174532925199432957692236907684886f / 1000.0f)
#define TREMOR_THRESHOLD 0.1f
#define TREMOR_FREQ_START 5    // 3 Hz
#define TREMOR_FREQ_END 10      // 6 Hz

#define GRAPH_PADDING 10
#define GRAPH_HEIGHT 100  // Height of the graph
#define GRAPH_WIDTH (lcd.GetXSize() - 2 * GRAPH_PADDING)
#define GRAPH_BOTTOM (lcd.GetYSize() - GRAPH_PADDING)
#define MAX_BAR_WIDTH (GRAPH_WIDTH / FFT_SIZE)

#define MAX_PRINT_HEIGHT 20  // The maximum number of rows to print in the console
#define SCALE_FACTOR 1.0      // Scaling factor to adjust magnitude values to fit console display