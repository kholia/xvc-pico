#define PWD0_PIN 2
#define PWD1_PIN 3
#define PRD0_PIN 4
#define PRD1_PIN 5

#define PCK_PIN 10
#define PWRITE_PIN 11
#define PWAIT_PIN 12

#define AXM_ITF 0

// typedef uint8_t cmd_buffer[64];
// // [0]     W/R#
// // [2:1]   LEN (10bit)
// // [7:4]   ADDRESS
// // [63:8]  DATA or [63:0]

void pmod_task();
