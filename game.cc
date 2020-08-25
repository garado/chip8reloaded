#include <iostream>
#include <stdio.h>
#include "led-matrix.h"
#include "graphics.h"
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <deque>

using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * chip8.h
 * Contains chip8 core implementation
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// CPU core implementation
class Chip8 {
  private:
    unsigned short opcode;    // 35 2-byte opcodes
    unsigned char V[16];      // 15 8bit GP registers V0-VE; 16th reg is carry flag
    unsigned short I;         // Index register (0x000 - 0xFFF)
    unsigned short pc;        // Program counter (0x000 - 0xFFF)
    unsigned short stack[16]; // Stack stuff
    unsigned short sp;
    unsigned char key[16];    // Hex-based keypad; array stores current state of key (1 = pressed)
    
    // MEMORY MAP
    // Chip8 has 4kb memory; each address contains 2bytes
    // 0x000-0x1FF - Chip 8 interpreter (contains font set in emu)
    // 0x050-0x0A0 - Used for the built in 4x5 pixel font set (0-F)
    // 0x200-0xFFF - Program ROM and work RAM
    unsigned char memory[4096];
    unsigned char delay_timer;  // 60Hz
    unsigned char sound_timer;

  public:
    unsigned short drawFlag;
    unsigned char gfx[64][32];    // Graphics: 64x32px B&W screen; array holds pixel state (1 or 0)
    std::deque<std::pair<int, int>> graphics;
    bool clearScreen;

    void init();
    void loadGame();
    void emulate();
    void setKeys(const char key);
    void timerTick();
};

/* * * * * * * * * * * * * * * * * * *
 * chip8.cpp
 * Chip8 & SDL function definitions
 * * * * * * * * * * * * * * * * * * */
/* :::::::::::: CHIP8 :::::::::::: */

// Key press constants
enum KeyPress {
  KEY_PRESS_1,
  KEY_PRESS_2,
  KEY_PRESS_3,
  KEY_PRESS_4,
  KEY_PRESS_Q,
  KEY_PRESS_W,
  KEY_PRESS_E,
  KEY_PRESS_R,
  KEY_PRESS_A,
  KEY_PRESS_S,
  KEY_PRESS_D,
  KEY_PRESS_F,
  KEY_PRESS_Z,
  KEY_PRESS_X,
  KEY_PRESS_C,
  KEY_PRESS_V,
};

// CHIP-8 fontset: each item is 4px wide, 5px high
unsigned char fontset[80] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// EMULATOR OPERATIONS --------------------------------

#define PRESSED 1
#define UNPRESSED 0
#define DEFAULT_CPU_RATE 9
#define DEFAULT_SLEEP_TIME 1

// Initializes the system by clearing memory, display, and registers.
void Chip8::init()
{
  pc = 0x200;
  sp = 0;
  I = 0;
  opcode = 0;
  drawFlag = false;

  // Clear display
  for (int i = 0 ; i < 64 ; i++) {
    for (int j = 0 ; j < 32 ; j++) {
      gfx[i][j] = 0;
    }
  }

  // Clear stack + registers
  for (int i = 0 ; i < 16 ; i++) {
    stack[i] = 0;
    V[i] = 0;
  }

  // Clear memory
  for (int i = 0 ; i < 4096 ; i++) {
    memory[i] = 0;
  }

  // Load fontset
  for (int i = 0 ; i < 80 ; i++) {
    memory[0x050 + i] = fontset[i];
  }

  return;
}

// Select and load game into program memory
void Chip8::loadGame()
{
  // Game select
  std::string games[] = { "15Puzzle", "Blinky", "Blitz", "Brix", "Chipquarium", "Connect4", "GlitchGhost", "Guess", 
          "Hidden", "Invaders", "Kaleid", "Maze", "Merlin", "Missile", "Octo", "Octorancher", "Pong1", 
          "Pong2", "Puzzle", "Snake", "Spaceflight", "Syzygy", "Tank", "Tetris", "Tictac",
          "UFO", "Vbrix", "Vers", "Wipeoff" };
  int gameCount = sizeof(games)/sizeof(games[0]);

  std::cout << "\u001b[31mSelect a game or press ESC to quit\u001b[0m" << std::endl;
  for (int i = 0; i < gameCount; i++) {
    std::cout << "[" << i << "]" << "\t" << games[i] << std::endl;
  }

  // Get valid user input
  int usrSelect;
  while (1) {
    std::cin >> usrSelect;
    if (!std::cin.fail() && usrSelect < gameCount) {
      break;
    } else {
      std::cout << "\u001b[31mEnter a valid integer input (0-" << gameCount - 1 << ")\u001b[0m" << std::endl;
      std::cin.clear();
      std::cin.ignore(1000, '\n');
    }
  }

  std::string gameSelect = "roms/" + games[usrSelect];
  FILE * game = fopen(gameSelect.c_str(), "rb");
  if (game == NULL) {
    std::cout << "ERROR: loadgame() cannot open file " << gameSelect << std::endl;
    exit(EXIT_FAILURE);
  }

  // Finding length of program
  fseek(game, 0L, SEEK_END);
  int gameSize = ftell(game);
  rewind(game);

  // Copy program into buffer
  char* buffer;
  buffer = (char*) calloc(gameSize + 1, sizeof(unsigned char));
  fread(buffer, gameSize, 1, game);

  // Start filling at memory location 0x200 = 512
  for (int i = 0 ; i < gameSize ; i++) {
    memory[0x200 + i] = buffer[i];
  }

  free(buffer);
  fclose(game);
}

// Emulates one cycle of the CPU
void Chip8::emulate()
{
  // Fetch opcode
  // PC is 4 bytes, but an address holds 2 bytes. Have to merge parts of PC together.
  opcode = memory[pc] << 8 | memory[pc + 1];

  unsigned short X, Y, N;
  X = (opcode & 0x0F00) >> 8;
  Y = (opcode & 0x00F0) >> 4;

  // Decode/execute opcode (35 total 2 byte opcodes)
  switch (opcode & 0xF000)
  {
    case 0x0000:
      switch (opcode)
      {
        case 0x00EE: // 00EE: returns from a subroutine
          sp--;
          pc = stack[sp];
          pc += 2;
          break;

        case 0x00E0: // 00E0: clears the screen
          for (int i = 0 ; i < 64 ; i++) {
            for (int j = 0 ; j < 32 ; j++) {
              gfx[i][j] = 0;
            }
          }
          clearScreen = true;
          pc += 2;
          break;

        default: // 0NNN: calls RCA 1082 program at address NNN
          std::cout << "Oh no! Unknown opcode 0x" << std::hex << opcode << std::endl;
          exit(EXIT_FAILURE);
          break;
      }
      break;

    case 0x1000: // 1NNN: jumps to address NNN
      pc = opcode & 0x0FFF;
      break;

    case 0x2000: // 0X2NNN: calls subroutine at NNN
      stack[sp] = pc;
      sp++;
      pc = opcode & 0x0FFF;
      break;

    case 0x3000: // 3XNN: skips the next instruction if vx == NN
      N = opcode & 0x00FF;
      if (V[X] == N) {
        pc += 4;
      } else {
        pc += 2;
      }
      break;

    case 0x4000: // 4XNN: skips the next instruction if VX != NN
      N = opcode & 0x00FF;
      if (V[X] != N) {
        pc += 4;
      } else {
        pc += 2;
      }
      break;

    case 0x5000: // 5XY0: skips the next instruction if VX == VY
      if (V[X] == V[Y]) {
        pc += 4;
      } else {
        pc += 2;
      }
      break;

    case 0x6000: // 6XNN: sets VX to NN
      V[X] = opcode & 0x00FF;
      pc += 2;
      break;

    case 0x7000: // 7XNN: adds nn to vx
      V[X] += opcode & 0x00FF;
      pc += 2;
      break;

    case 0x8000:
      pc += 2;
      switch (opcode & 0x000F)
      {
        case 0: // 8XY0: VX = VY
          V[X] = V[Y];
          break;

        case 1: // 8XY1: VX = VX | VY
          V[X] |= V[Y];
          break;

        case 2: // 8XY2: VX = VX & VY
          V[X] &= V[Y];
          break;

        case 3: // 8XY3: VX = VX ^ VY
          V[X] ^= V[Y];
          break;

        case 4: // 8XY4: VX = VX + VY; VF = 1 if carry ( > 255 ), 0 if not
          if (V[X] + V[Y] > 255) {
            V[0xF] = 1;   // Carry
          } else {
            V[0xF] = 0;
          }
          V[X] += V[Y];
          break;

        case 5: // 8XY5: VX = VX - VY; VF = 0 if borrow, 1 if not
          if (V[Y] > V[X]) {
            V[0xF] = 0;   // Borrow
          } else {
            V[0xF] = 1;
          }
          V[X] -= V[Y];
          break;

        case 6: // 8XY6: LSB of VX in VF; shifts VX right by 1
          V[0xF] = V[X] & 0x01;
          V[X] >>= 1;
          break;

        case 7: // 8XY7: VX = VY - VX; VF = 0 if borrow (goes negative), 1 if none
          if (V[X] > V[Y]) {
            V[0xF] = 0;
          } else {
            V[0xF] = 1;
          }
          V[X] = V[Y] - V[X];
          break;

        case 0xE: // 8XYE: MSB of VX in VF; shift VX left by 1
          V[0xF] = V[X] & 0x80;
          V[X] <<= 1;
          break;

        default:
          std::cout << "Oh no! Unknown opcode 0x" << std::hex << opcode << std::endl;
          exit(EXIT_FAILURE);
          break;
      }
      break;

    case 0x9000: // 9XY0: skip next instr if VX != VY
      if (V[X] != V[Y]) {
        pc += 4;
      } else {
        pc += 2;
      }
      break;

    case 0xA000: // ANNN: sets I to the address NNN
      I = opcode & 0x0FFF;
      pc += 2;
      break;

    case 0xB000: // BNNN: jumps to the address NNN plus V0
      pc = V[0] + (opcode & 0x0FFF);
      break;

    case 0xC000: // CXNN: sets VX to the result of random number & NN
      V[X] = (rand() & 0xFF) & (opcode & 0x00FF);
      pc += 2;
      break;

    case 0xD000: // DXYN: draws sprite
      // Draws sprite at coordinate VX, VY with width 8px, height Npx
      // Each row of 8 pixels is read as bit-coded starting from mem location I
      // I doesn't change after execution of instruction
      // VF = 1 if collisions occur, 0 if none
      {
        unsigned short height = opcode & 0x000F;
        unsigned short pixel, posX, posY;
        V[0xF] = 0;

        for (int yCoord = 0 ; yCoord < height ; yCoord++) {
          pixel = memory[I + yCoord]; // reads from memory location I + ycoord

          for (int xCoord = 0 ; xCoord < 8 ; xCoord++) {

            if (pixel & (0x80 >> xCoord)) { // Cycles all 8px of width
              posX = (V[X] + xCoord);
              posY = (V[Y] + yCoord);

              if (gfx[posX][posY] == 1) {   // Check for collisions
                V[0xF] = 1;
              }

              gfx[posX][posY] ^= 1;             // Draw to screen
              graphics.push_back({posX, posY});
            }
          }
        }

        drawFlag = true;
        pc += 2;
      }
      break;

    case 0xE000: // keyops
      switch (opcode & 0x00FF) {
        case 0x9E: // EX9E: skips next instr if key stored in VX is pressed
          if (key[V[X]] == PRESSED) {
            pc += 4;
            key[V[X]] = UNPRESSED;
          } else {
            pc += 2;
          }
          break;

        case 0xA1: // EXA1: skips next instr if the key stored in VX isn't pressed
          if (key[V[X]] == UNPRESSED) {
            pc += 4;
          } else {
            pc += 2;
            key[V[X]] = UNPRESSED;
          }
          break;

        default:
          std::cout << "Oh no! Unknown opcode 0x" << std::hex << opcode << std::endl;
          exit(EXIT_FAILURE);
          break;
      }
      break;

    case 0xF000:
      switch (opcode & 0x00FF) {
        case 0x0007: // FX07: sets VX to value of delay timer
          V[X] = delay_timer;
          pc += 2;
          break;

        case 0x000A: // FX0A: a keypress is awaited, then stored in V[X]
          {
            bool keypress = false;
            for (int i = 0 ; i < 16 ; i++) {
              if (key[i]) {
                V[X] = i;
                keypress = true;
              }
            }

            if (!keypress) {
              return;
            }

            pc += 2;
            break;
          }

        case 0x0015: // FX15: sets delay timer to VX
          delay_timer = V[X];
          pc += 2;
          break;

        case 0x0018: // FX18: sets sound timer to VX
          sound_timer = V[X];
          pc += 2;
          break;

        case 0x001E: // FX1E: adds VX to I; VF = 1 if range overflow, 0 if not
          if ((V[X] + I) > 0xFFF) {
            V[0xF] = 1;
          } else {
            V[0xF] = 0;
          }
          I += V[X];
          pc += 2;
          break;

        case 0x0029: // FX29: sets I = location of sprite in mem for the character in VX
          I = V[X] * 5;
          pc += 2;
          break;

        case 0x0033: // FX33
          // stores binary coded decimal representation of VX
          // most significant digit in mem location at I
          // middle digit in I + 1
          // least sig at address I + 2
          {
            unsigned short msb = V[X] / 100;
            unsigned short lsb = V[X] % 10;
            unsigned short mid = (V[X] % 100 - lsb) % 10;
            memory[I] = msb;
            memory[I + 1] = mid;
            memory[I + 2] = lsb;
            pc += 2;
          }
          break;

        case 0x0055: // FX55
          // stores V0 to VX (including VX) in mem starting at address I
          // offset from I increased by 1 for each value written, but I isn't modified
          for (int i = 0; i <= X; i++) {
            memory[I + i] = V[i];
          }
          // I += X + 1; // The original implementation had I += X + 1
          pc += 2;
          break;

        case 0x0065: // FX65
          // fills V0 to VX (including VX) with values from mem starting at address I
          // offset from I increased by 1 for each value written, but I isn't modified
          for (int i = 0; i <= X; i++) {
            V[i] = memory[I + i];
          }
          // I += X + 1;   // Original implementation had I += X + 1
          pc += 2;
          break;
      }
      break;

    default:
      std::cout << "Oh no! Unknown opcode 0x" << std::hex << opcode << std::endl;
      exit(EXIT_FAILURE);
  }
}

// Determines keypresses
void Chip8::setKeys(const char keypress) {
  switch (keypress) {
  case '1': key[0x1] = PRESSED; break;
  case '2': key[0x2] = PRESSED; break;
  case '3': key[0x3] = PRESSED; break;
  case '4': key[0xC] = PRESSED; break;
  case 'q': key[0x4] = PRESSED; break;
  case 'w': key[0x5] = PRESSED; break;
  case 'e': key[0x6] = PRESSED; break;
  case 'r': key[0xD] = PRESSED; break;
  case 'a': key[0x7] = PRESSED; break;
  case 's': key[0x8] = PRESSED; break;
  case 'd': key[0x9] = PRESSED; break;
  case 'f': key[0xE] = PRESSED; break;
  case 'z': key[0xA] = PRESSED; break;
  case 'x': key[0x0] = PRESSED; break;
  case 'c': key[0xB] = PRESSED; break;
  case 'v': key[0xF] = PRESSED; break;
  default: break;
  }
}

// Delay & sound timers are 60Hz
void Chip8::timerTick() {
  if (delay_timer > 0) {
      delay_timer--;
  }

  if (sound_timer > 0) {
    sound_timer--;
    if (sound_timer == 1) {
      // Mix_PlayChannel(-1, beep, 0);
    }
  }
}

volatile bool interrupt_received = false;
static void InterruptHandler(int signo) {
  interrupt_received = true;
}

// Retrieve input from cmdline (nonblocking!)
static char getch() {
  static bool is_terminal = isatty(STDIN_FILENO);

  struct termios old;
  if (is_terminal) {
    if (tcgetattr(0, &old) < 0)   // stores details abt terminal in termios
      perror("tcsetattr()");

    // Set to unbuffered mode
    struct termios no_echo = old; 
    no_echo.c_lflag &= ~ICANON; // canonical input (erase + kill processing)
    // no_echo.c_lflag &= ~ECHO;   // input chars echoed back to terminal
    no_echo.c_cc[VMIN] = 0;
    no_echo.c_cc[VTIME] = 0.1;
    if (tcsetattr(0, TCSANOW, &no_echo) < 0)
      perror("tcsetattr ICANON");
  }

  char buf = 0;
  if (read(STDIN_FILENO, &buf, 1) < 0)
    perror ("read()");

  if (is_terminal) {
    // Back to original terminal settings.
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
      perror ("tcsetattr ~ICANON");
  }

  return buf;
}

// MAIN BITCH
int main(int argc, char* argv[]) {
  int cpuSpeed = DEFAULT_CPU_RATE;
  int sleepTime = DEFAULT_SLEEP_TIME;
  int red = 121;
  int green = 125;
  int blue = 98;

  // Retrieves command line options
  // -t <speed>   timer speed
  // -d <time>    timer delay ( input * 1000 )
  // -c <r,g,b>   display color
  int opt;
  while ((opt = getopt(argc, argv, "t:d:c:")) != -1) {
    switch (opt) {
    case 't':
      cpuSpeed = atoi(optarg);
      break;
    case 'd':
      sleepTime = atoi(optarg);
      break;
    case 'c':
      sscanf(optarg, "%d,%d,%d", &red, &green, &blue);
      break;
    default:
      break;
    }
  }
  
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "regular";
  defaults.rows = 32;
  defaults.cols = 64;
  defaults.brightness = 50;
  defaults.chain_length = 1;
  defaults.parallel = 1;
  Canvas *canvas = RGBMatrix::CreateFromFlags(&argc, &argv, &defaults);
  if (canvas == NULL)
    return 1;

  Chip8 emulator;
  emulator.init();
  emulator.loadGame();
  // bool load = true;

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);
  bool running = true;

  while (!interrupt_received && running) {
    // Load (or reload) (currently broken)
    // if (load) {
    //   canvas->Clear();
    //   emulator.init();
    //   emulator.loadGame();
    //   load = false;
    // }

    // Prevents user's keypresses from spamming the terminal
    const bool output_is_terminal = isatty(STDOUT_FILENO);
    printf("%s%s",
           output_is_terminal ? "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b" : "",
           output_is_terminal ? " " : "\n");
    fflush(stdout);

    // Graphics handling
    if (emulator.clearScreen) {
      canvas->Clear();
      emulator.clearScreen = false;
    }

    if (emulator.drawFlag) {
      for (const auto &pos : emulator.graphics) {
        if (emulator.gfx[pos.first][pos.second]) {
          canvas->SetPixel(pos.first, pos.second, red, green, blue);
        } else {
          canvas->SetPixel(pos.first, pos.second, 0, 0, 0);
        }
      }
    }

    // Extremely wonky timing + keypress stuff
    for (int i = 0; i < cpuSpeed; i++) {
      const char keypress = tolower(getch());

      // if (keypress == 0x1B) {   // Esc: return to game select menu
      //   load = true;
      //   break;
      // }

      emulator.setKeys(keypress);
      emulator.emulate();
    }
    emulator.timerTick();
    usleep((sleepTime * 1000));
  }

  std::cout << "\nSee you later! Thanks for playing." << std::endl;
  canvas->Clear();
  delete canvas;
  exit(EXIT_SUCCESS);
}
