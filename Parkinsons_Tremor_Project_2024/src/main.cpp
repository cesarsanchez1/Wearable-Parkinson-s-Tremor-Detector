// Team members
// Sai Spoorthi Tammineni st5294
// Cesar Sanchez	cs5564
// linyuan	zhang	lz3168

#include "headers.h"

//Screen settings 

// LCD screen object
LCD_DISCO_F429ZI lcd;

// buffer for holding displayed text strings
char display_buf[2][60];

// Width and height of the graph area on the screen
uint32_t graph_width = lcd.GetXSize() - 2 * GRAPH_PADDING;
uint32_t graph_height = graph_width;

// sets the background layer to be visible, transparent, and resets its colors to all black
void setup_background_layer()
{
  lcd.SelectLayer(BACKGROUND);
  lcd.Clear(LCD_COLOR_BLACK);
  lcd.SetBackColor(LCD_COLOR_BLACK);
  lcd.SetTextColor(LCD_COLOR_GREEN);
  lcd.SetLayerVisible(BACKGROUND, ENABLE);
  lcd.SetTransparency(BACKGROUND, 0x7Fu);
}

// resets the foreground layer to all black
void setup_foreground_layer()
{
  lcd.SelectLayer(FOREGROUND);
  lcd.Clear(LCD_COLOR_BLACK);
  lcd.SetBackColor(LCD_COLOR_BLACK);
  lcd.SetTextColor(LCD_COLOR_LIGHTGREEN);
}


//spi flag settings
EventFlags flags;

void spi_cb(int event) {
    flags.set(SPI_FLAG);
}

DigitalOut led(LED1);  // Initialize the LED

//helper function
void helper(float *real, float *imag, int n) {
    int j = 0;
    for (int i = 0; i < n; ++i) {
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
}


void print_fft_graph(float *fft_output, int fft_size) {
    // Determine the maximum value in fft_output for scaling purposes
    float max_value = 0;
    for (int i = 0; i < fft_size; i++) {
        if (fft_output[i] > max_value) {
            max_value = fft_output[i];
        }
    }

    for (int row = MAX_PRINT_HEIGHT; row > 0; row--) {
        for (int col = 0; col < fft_size; col++) {
            float scaled_height = (fft_output[col] / max_value * MAX_PRINT_HEIGHT);
            if (scaled_height >= row) {
                std::cout << "*";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "\n";
    }

    // Print a base line
    for (int i = 0; i < fft_size; i++) {
        std::cout << "-";
    }
    std::cout << "\n";
}

// Fast Fourier Transform function
#include <cmath>

void performFFT(float *realParts, float *imaginaryParts, int length) {
    // Pre-process using a helper function if necessary (ensure it's defined elsewhere)
    helper(realParts, imaginaryParts, length);

    // Perform the FFT using the iterative Cooley-Tukey algorithm
    for (int level = 1; level <= log2(length); ++level) {
        int span = 1 << level;
        float unityRootReal = cos(2 * M_PI / span);
        float unityRootImag = -sin(2 * M_PI / span);
        
        for (int segmentStart = 0; segmentStart < length; segmentStart += span) {
            float omegaReal = 1.0;
            float omegaImag = 0.0;
            
            for (int pos = 0; pos < span / 2; ++pos) {
                int upperPos = segmentStart + pos;
                int lowerPos = segmentStart + pos + span / 2;
                
                float tempReal = omegaReal * realParts[lowerPos] - omegaImag * imaginaryParts[lowerPos];
                float tempImag = omegaReal * imaginaryParts[lowerPos] + omegaImag * realParts[lowerPos];
                
                realParts[lowerPos] = realParts[upperPos] - tempReal;
                imaginaryParts[lowerPos] = imaginaryParts[upperPos] - tempImag;
                
                realParts[upperPos] += tempReal;
                imaginaryParts[upperPos] += tempImag;
                
                // Update omega for next iteration
                float temp = omegaReal * unityRootReal - omegaImag * unityRootImag;
                omegaImag = omegaReal * unityRootImag + omegaImag * unityRootReal;
                omegaReal = temp;
            }
        }
    }
}


int tremor_counter = 0;
// Function to detect tremors based on FFT output
bool detect_tremor(float *fft_output, int size, float &tremor_intensity) {
    tremor_intensity = 0.0f;
    bool tremor_detected = false;

    for (int i = TREMOR_FREQ_START; i <= TREMOR_FREQ_END; i++) {
        if (fft_output[i] > TREMOR_THRESHOLD) {
            tremor_detected = true;
            tremor_intensity += fft_output[i];
        }
    }

    if(tremor_intensity < 15 || tremor_intensity == false){
        return false;
    }
    // printf("TESTING INTERMEDIATE RESULTS:  Intensity = %4.5f\n", tremor_intensity);
    tremor_intensity = tremor_intensity - 9.0;
    if(tremor_intensity >= 100){
        tremor_intensity = 100;
    }

    return tremor_detected;
}



int main() {
      // Set up the background and foreground layers of the LCD screen
    setup_background_layer();
    setup_foreground_layer();
    lcd.SelectLayer(FOREGROUND);                                                    // Select the foreground layer for drawing
    lcd.DisplayStringAt(0, LINE(17), (uint8_t *)display_buf[0], LEFT_MODE);       // setting up the stm32 display board
    lcd.SelectLayer(FOREGROUND);
    SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);                             // Initialize the SPI object with specific pins.
    
    float input[SAMPLES];
    float imag[SAMPLES];
    int sample_index = 0;

    // Buffers for sending and receiving data over SPI.
    int8_t write[32], read[32];
    spi.format(8, 3);
    spi.frequency(1'000'000);

    // Configure CTRL_REG1 register.
    write[0] = CTRL_REG1;
    write[1] = CTRL_REG1_CONFIG;
    spi.transfer(write, 2, read, 2, spi_cb);
    flags.wait_all(SPI_FLAG);

    // Configure CTRL_REG4 register.
    write[0] = CTRL_REG4;
    write[1] = CTRL_REG4_CONFIG;
    spi.transfer(write, 2, read, 2, spi_cb);
    flags.wait_all(SPI_FLAG);

    while(1) {
        int16_t rawgx, rawgy, rawgz;
        float gx, gy, gz;
        write[0] = OUT_X_L | 0x80 | 0x40;

        // Perform the SPI transfer to read 6 bytes of data (for x, y, and z axes)
        spi.transfer(write, 7, read, 7, spi_cb);
        flags.wait_all(SPI_FLAG);

        // Convert the received data into 16-bit integers for each axis
        rawgx = (((int16_t)read[2]) << 8) | ((int16_t) read[1]);
        rawgy = (((int16_t)read[4]) << 8) | ((int16_t) read[3]);
        rawgz = (((int16_t)read[6]) << 8) | ((int16_t) read[5]);


        // Convert raw data to actual values using a scaling factor
        gx = ((float) rawgx) * SCALING_FACTOR;
        gy = ((float) rawgy) * SCALING_FACTOR;
        gz = ((float) rawgz) * SCALING_FACTOR;

        
        input[sample_index++] = gz;
        if (sample_index >= SAMPLES) {
            sample_index = 0;
            memset(imag, 0, sizeof(imag));

            // Perform the FFT
            performFFT(input, imag, SAMPLES);

            // Calculate the magnitude of the FFT output
            float fft_output[FFT_SIZE];
            for (int i = 0; i < FFT_SIZE; i++) {
                fft_output[i] = sqrtf(input[i] * input[i] + imag[i] * imag[i]);
            }

            printf("\n\n");
            // Call this function in your main loop where the FFT output is ready
            print_fft_graph(fft_output, FFT_SIZE);
            // Detect tremors
            float tremor_intensity;
            if (detect_tremor(fft_output, FFT_SIZE, tremor_intensity)) {
                printf("Tremor detected!\n");
                printf("Intensity: %d\n\n", static_cast<int>(ceil(tremor_intensity)));
                led = 1;    //led on
                snprintf(display_buf[6],60,"Tremor Detected     ");  
                lcd.SetTextColor(LCD_COLOR_RED);                                  
                lcd.DisplayStringAt(0, LINE(10), (uint8_t *)display_buf[6], LEFT_MODE);
                
            } else {
                printf("No tremor detected.\n\n");
                led = 0;    //led off
                snprintf(display_buf[6],60,"No Tremor Detected"); 
                lcd.SetTextColor(LCD_COLOR_GREEN);                                   
                lcd.DisplayStringAt(0, LINE(10), (uint8_t *)display_buf[6], LEFT_MODE);
                
            }
        }

        thread_sleep_for(20);
    }
}
