#define SDRAM_BASE            0xC0000000
#define FPGA_ONCHIP_BASE      0xC8000000
#define FPGA_CHAR_BASE        0xC9000000

/* Cyclone V FPGA devices */
#define LEDR_BASE             0xFF200000
#define HEX3_HEX0_BASE        0xFF200020
#define HEX5_HEX4_BASE        0xFF200030
#define SW_BASE               0xFF200040
#define KEY_BASE              0xFF200050
#define TIMER_BASE            0xFF202000
#define PIXEL_BUF_CTRL_BASE   0xFF203020
#define CHAR_BUF_CTRL_BASE    0xFF203030

/* VGA colors */
#define WHITE 0xFFFF
#define YELLOW 0xFFE0
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define GREY 0xC618
#define PINK 0xFC18
#define ORANGE 0xFC00

#define ABS(x) (((x) > 0) ? (x) : -(x))
	
#define true 1
#define false 0


/* Move Directions */
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
	
/* X and Y size of the grid*/
#define GRID_Y	12
#define GRID_X	16
	
#define GRID_START_OFFSET_X 2
#define GRID_START_OFFSET_Y 1


/* size of grid in amount of text */
#define GRID_TEXT_SIZE 4
#define LINE_PER_GRID 4
#define GRID_SIZE_X 4*GRID_TEXT_SIZE
/* top line is type, bottom line is unit count */
#define GRID_SIZE_Y 4*LINE_PER_GRID
#define MAX_UNIT 99
	
#define GRID_START_X GRID_START_OFFSET_X*GRID_SIZE_X
#define GRID_START_Y GRID_START_OFFSET_Y*GRID_SIZE_Y
#define GRID_END_X GRID_SIZE_X*GRID_X+GRID_START_X
#define GRID_END_Y GRID_SIZE_Y*GRID_Y+GRID_START_Y
	
#define GRID_COLOR WHITE
	
#define BACKGROUND_COLOR 0x0000
	
/*tile types*/
#define EMPTY 0
#define MOUNTAIN 1
#define BASE 2
#define TOWER 3
	
/*factions*/
#define NONE 0
#define FIRST 1
#define SECOND 2
	
/*Color*/
#define FIRST_COLOR BLUE
#define SECOND_COLOR RED
	
#define FIRST_SELECT_COLOR CYAN
#define SECOND_SELECT_COLOR MAGENTA
	
#include <stdlib.h>
#include <stdio.h>
	
volatile int pixel_buffer_start; // global variable

void initializeBuffer();
void plot_pixel(int, int, short int);
void swap(int *, int *);

void clear_screen();
void draw_line(int x0, int y0, int x1, int y1, short int color);

void wait_sync();
void drawGrid();
void doRender();
void doGameTick();
void setup();
void setupGrid();
void drawUnitCount(int gridX, int gridY);
void drawTileType(int gridX, int gridY);
void drawTileFaction(int gridX, int gridY);
void drawDigit(int x, int y, int val);
void drawAscii(int x, int y, char val);
void drawSelection(int side);
int getMoveDirection(int keyCode);
void drawHighlight(int x, int y, int side);
void flashHighlight(int side);
void tryMoveUnit(int side, int x, int y, int direction);
int get_random(int limit);
void generateChipTune();
void renderText(int x, int y, char* text);
void drawHelp();

int unitCount[GRID_X][GRID_Y];
int animatedUnitCount[GRID_X][GRID_Y];
int gridTerrain[GRID_X][GRID_Y];
int tileFaction[GRID_X][GRID_Y];

int playerOneSelectX;
int playerOneSelectY;

int playerTwoSelectX;
int playerTwoSelectY; 

int isPlayerOneSelecting;
int isPlayerTwoSelecting;

unsigned char PS2Bytes[3];	//ps2 packets are always at most 3 bytes

int currentTrueTick;
int currentCalculatedTick;

int playerOnePressShift;
int playerTwoPressShift;

int gameEnded = false;
int needInitialize = false;
int needAnimation = false;

volatile int * LEDAddress = (int *)0xff200000;

/* ↓↓↓ Assembly Execution Helpers ↓↓↓ */

void cpsr_msr(int value);
void mov_sp(int value);
void toggleIRQ(int isIRQ);
void setupStackPointer();
void setupGIC();
void enableInterruptFor(int irqID);
void showWinningSide(int side);
void initializeRandomizer();
void writeAudio(double freq, int samplesToGenerate);
	
void cpsr_msr(int value){
	__asm__("msr cpsr, %[ps]"::[ps]"r"(value));
}

void mov_sp(int value){
	__asm__("mov sp, %[ps]"::[ps]"r"(value));
}

void enableInterrupt(){
	int status = 0b01010011;	
	__asm__("msr cpsr, %[ps]"::[ps]"r"(status));
}

void disableInterrupt(){
	int status = 0b11010011;	
	__asm__("msr cpsr, %[ps]"::[ps]"r"(status));
}

void setupStackPointer(){
	int mode, stack;
	mode = 0b11010010;
	__asm__("msr cpsr, %[ps]"::[ps]"r"(mode));
	stack = 0xFFFFFFFF-7;
	__asm__("mov sp, %[ps]"::[ps]"r"(stack));
	mode = 0b11010011;
	__asm__("msr cpsr, %[ps]"::[ps]"r"(mode));
}

void setupGIC(){
	// Set Interrupt Priority Mask Register (ICCPMR). Enable all priorities
	*((int *) 0xFFFEC104) = 0xFFFF;
	// Set the enable in the CPU Interface Control Register (ICCICR)
	*((int *) 0xFFFEC100) = 1;
	// Set the enable in the Distributor Control Register (ICDDCR)
	*((int *) 0xFFFED000) = 1;
}

void config_interrupt (int N, int CPU_target)
{
	int reg_offset, index, value, address;
	/* Configure the Interrupt Set-Enable Registers (ICDISERn).
	* reg_offset = (integer_div(N / 32) * 4; value = 1 << (N mod 32) */
	reg_offset = (N >> 3) & 0xFFFFFFFC;
	index = N & 0x1F;
	value = 0x1 << index;
	address = 0xFFFED100 + reg_offset;
	/* Using the address and value, set the appropriate bit */
	*(int *)address |= value;
	/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
	* reg_offset = integer_div(N / 4) * 4; index = N mod 4 */
	reg_offset = (N & 0xFFFFFFFC);
	index = N & 0x3;
	address = 0xFFFED800 + reg_offset + index;
	/* Using the address and value, write to (only) the appropriate byte */
	*(char *)address = (char) CPU_target;
}

void enableInterruptFor(int irqID){
	//printf("Enabled interrupt for: %d\n", irqID);
	/*//each interrupt set-enable register hold 32 flags in total, therefore
	//the # register for a device is floor(irqID / 32).
	int irqAddrOffset = (irqID/32) * 4;
	//The exact bit that is flag for the device in that address
	//is just how far the id is from a multiple of 32, and
	//use bit-shift to make that bit 1
	int regVal = 1 << (irqID%32);
	int ICDISER_BaseAddress = 0xFFFED100;
	//use bit-wise or to not break other data
	*((int *)(ICDISER_BaseAddress + irqAddrOffset)) |= regVal;
	
	//similar process for interrupt processor target register
	//but since they only hold 4 value each
	//the # reg will be floor(irqID/4) and offset will be the distance to a multiple of 4
	int irqCpuAddrOffset = (irqID/4);
	int cpuRegVal = (irqID%4);
	int ICDIPTR_Base_Address = 0xFFFED800;
	//since it is 8 bit (size of char) this time we dont need to use bitwise or
	*((char *)(ICDIPTR_Base_Address + irqCpuAddrOffset + cpuRegVal)) = (char) 1;	//the chip is always cpu 1*/
	config_interrupt(irqID, 1);
}

/* ↑↑↑ Assembly Execution Helpers ↑↑↑ */

////////////////////////////////////////////

/* ↓↓↓ Exception Vector Table ↓↓↓ */
void handleIRQ(int irqID);

void __attribute__ ((interrupt)) __cs3_isr_irq (){
	// Read the ICCIAR from the CPU Interface in the GIC
	int interrupt_ID = *((int *) 0xFFFEC10C);
	
	handleIRQ(interrupt_ID);
	
	*((int *) 0xFFFEC110) = interrupt_ID;
	return;
}
// Define the remaining exception handlers
void __attribute__ ((interrupt)) __cs3_reset (){
	return;
}
void __attribute__ ((interrupt)) __cs3_isr_undef (){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_swi (){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_pabort (){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_dabort (){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_fiq (){
	while(1);
}

/* ↑↑↑ Exception Vector Table ↑↑↑ */


/* ↓↓↓ IRQ Handler ↓↓↓ */
void irqSetupMain();
void registerInterruptRequest();
void setupDeviceInterrupt();
void ps2IrqHandler();
void TimerIrqHandler();
void ButtonIrqHandler();
void AudioIrqHandler();
void AnimationTimerIrqHandler();

void irqSetupMain(){
	disableInterrupt();
	//printf("interrupt disabled\n");
	setupStackPointer();
	//printf("stack pointer setup done\n");
	registerInterruptRequest();
	//printf("interrupt registered\n");
	setupGIC();
	//printf("gic setup done\n");
	setupDeviceInterrupt();
	//printf("device interrupt setup done\n");
	enableInterrupt();
	//printf("interrupt enabled\n");
}

void registerInterruptRequest(){
	//add all needed interrupts here
	enableInterruptFor(79);	//PS2 keyboard
	enableInterruptFor(29);	//A9 private timer
	//enableInterruptFor(72);	//Interval timer
	enableInterruptFor(73);	//Button
	//enableInterruptFor(78);	//Audio
}

void setupDeviceInterrupt(){
	//add all device specific interrupt setup codes here	
	
	//PS2 controller
	volatile int * ps2Address = (int *)0xFF200104;
	//https://www-ug.eecg.toronto.edu/msl/nios_devices/dev_ps2.html
	*(ps2Address) = 1;		//interrupt enable
	
	
	//A9 timer
	volatile int * A9TimerAddress = (int *)0xfffec600;
	*A9TimerAddress = 50000000;
	*(int *)(A9TimerAddress + 2) = 0b110;
	*(int *)(A9TimerAddress + 3) = 1;
	
	//timer
	volatile int * IntervalTimerAddress = (int *) 0xff202000;
	*(int *)(IntervalTimerAddress + 2) = 0xffff;
	*(int *)(IntervalTimerAddress + 3) = 0x20;
	*(int *)(IntervalTimerAddress + 1) = 0b1011;
	*(int *)(IntervalTimerAddress + 0) = 0;
	
	//Button
	volatile int * ButtonAddress = (int *) 0xff200050;
	*(int *)(ButtonAddress + 2) = 0b1;
	*(int *)(ButtonAddress + 3) = 0b111;
	
	//Audio
	//volatile int * AudioAddress = (int *) 0xff203040;
	//*(int *)(AudioAddress) = 0b10;
	
}

void handleIRQ(int irqID){
	//printf("interrupt received:%d\n", irqID);
	if(irqID == 79){
		ps2IrqHandler();
	}else if(irqID == 29){
		TimerIrqHandler();
	}else if(irqID == 72){
		AnimationTimerIrqHandler();
	}else if(irqID == 73){
		ButtonIrqHandler();
	}else{
		while(1);	//unknown interrupt	
	}
	//printf("interrupt processed:%d\n", irqID);
}

void AnimationTimerIrqHandler(){
	needAnimation = true;
	volatile int * IntervalTimerAddress = (int *) 0xff202000;
	*(int *)(IntervalTimerAddress) |= 0b0;
}

void ButtonIrqHandler(){
	needInitialize = true;
	volatile int * ButtonAddress = (int *) 0xff200050;
	*(int *)(ButtonAddress + 3) = 0b111;
}

void TimerIrqHandler(){
	currentTrueTick += 1;		//add 1 to the game tick
	
	//reset timer and clear interrupt
	volatile int * A9TimerAddress = (int *)0xfffec600;
	*(int *)(A9TimerAddress + 3) = 1;
}

void ps2IrqHandler(){
	
	*LEDAddress |= 0b100;
	volatile int * PS2_ptr = (int *) 0xFF200100;
	int data = *(PS2_ptr);
	if((data & 0x8000) != 0){
		PS2Bytes[0] = PS2Bytes[1];
		PS2Bytes[1] = PS2Bytes[2];
		PS2Bytes[2] = data & 0xFF;
	}
	
	
	if(gameEnded == true) return;
	
	int moveDirection = NONE;
	int moveSide = NONE;
	
	//printf("%02x\n",PS2Bytes[2]);
	
	if(PS2Bytes[1] == 0xF0){
		if(PS2Bytes[2] == 0x12){
			//shift is first player
			playerOnePressShift = false;
		}else if(PS2Bytes[2] == 0x14){
			playerTwoPressShift = false;
		}
	}else if(PS2Bytes[2] == 0x5A){
		//enter	
		if(isPlayerTwoSelecting == true){
			isPlayerTwoSelecting = false;	
		}else{
			isPlayerTwoSelecting = true;	
		}
	}else if(PS2Bytes[2] == 0x29){
		//space
		if(isPlayerOneSelecting == true){
			isPlayerOneSelecting = false;	
		}else{
			isPlayerOneSelecting = true;	
		}
	}else if(PS2Bytes[2] == 0x12){
		playerOnePressShift = true;
	}else if(PS2Bytes[2] == 0x14){
		playerTwoPressShift = true;
	}else if(PS2Bytes[1] == 0xE0){
		moveDirection = getMoveDirection(PS2Bytes[2]);
		moveSide = SECOND;
	}else if(PS2Bytes[1] != 0xE0){
		moveDirection = getMoveDirection(PS2Bytes[2]);
		moveSide = FIRST;
	}
	if(moveDirection != NONE){
		if(moveSide == SECOND){
			if(isPlayerTwoSelecting){
				tryMoveUnit(SECOND, playerTwoSelectX, playerTwoSelectY, moveDirection);
			}else{
				switch(moveDirection){
					case UP:
						if(playerTwoSelectY > 0){
							playerTwoSelectY -= 1; 
						}
						break;
					case DOWN:
						if(playerTwoSelectY < GRID_Y - 1){
							playerTwoSelectY += 1;
						}
						break;
					case LEFT:
						if(playerTwoSelectX > 0){
							playerTwoSelectX -= 1;
						}
						break;
					case RIGHT:
						if(playerTwoSelectX < GRID_X - 1){
							playerTwoSelectX += 1;
						}
						break;
				}
			}
		}else if(moveSide == FIRST){
			if(isPlayerOneSelecting){
				tryMoveUnit(FIRST, playerOneSelectX, playerOneSelectY, moveDirection);
			}else{
				switch(moveDirection){
					case UP:
						if(playerOneSelectY > 0){
							playerOneSelectY -= 1;
						}
						break;
					case DOWN:
						if(playerOneSelectY < GRID_Y - 1){
							playerOneSelectY += 1;
						}
						break;
					case LEFT:
						if(playerOneSelectX > 0){
							playerOneSelectX -= 1;
						}
						break;
					case RIGHT:
						if(playerOneSelectX < GRID_X - 1){
							playerOneSelectX += 1;
						}
						break;
				}
			}
		}
	}
}

/* ↑↑↑ IRQ Handler ↑↑↑ */


///////////////////////////
int main(void)
{
	irqSetupMain();
	//printf("irq setup done\n");
	while(1){
		setup();
		while(needInitialize == false){
			doGameTick();
			doRender();
		}
	}
}

void setup(){
	volatile int * IntervalTimerAddress = (int *) 0xff202000;
	*(int *)(IntervalTimerAddress + 1) = 0b1011;
	volatile int * A9TimerAddress = (int *)0xfffec600;
	*(int *)(A9TimerAddress + 2) = 0b110;
	*LEDAddress |= 0b1;
	
	//printf("setup start\n");
	
	initializeBuffer();
	initializeRandomizer();
	initializeBuffer();
	setupGrid();
	drawHelp();
	gameEnded = false;
	needInitialize = false;
	//printf("setup done\n");
	
	*(int *)(IntervalTimerAddress + 1) = 0b0111;
	*(int *)(A9TimerAddress + 2) = 0b111;
	*LEDAddress |= 0b10;
}
//////////////////////////


/* ↓↓↓ Game Logic ↓↓↓ */

void initializeRandomizer(){
	char* Text = "User input needed to generate data entropy";
	renderText(2,2,Text);
	Text = "Turn on and off switch 0 to generate random map";
	renderText(2,3,Text);
	volatile int * Switch_ptr = (int *) 0xff200040;
	while(((*Switch_ptr) & 0b1) == 0){
		get_random(1);
	}
	while(((*Switch_ptr) & 0b1) == 1){
		get_random(1);
	}
}

int get_random(int limit){
	return rand() % (limit+1);
}

int getDistance(int x1, int y1, int x2, int y2){
	return ABS(x1 - x2) + ABS(y1 - y2);
}

void setupGrid(){
	currentTrueTick = 0;
	currentCalculatedTick = 0;
	for(int i = 0; i < GRID_X; i++){
		for(int j = 0; j < GRID_Y; j++){
			unitCount[i][j] = 0;
			animatedUnitCount[i][j] = unitCount[i][j];
			int randomValue = get_random(100);
			if(randomValue < 85){
				gridTerrain[i][j] = EMPTY;
			}else if(randomValue < 95){
				gridTerrain[i][j] = MOUNTAIN;
			}else{
				gridTerrain[i][j] = TOWER;
			}
			tileFaction[i][j] = NONE;
		}
	}
	int baseOneX = get_random(GRID_X-1);
	int baseOneY = get_random(GRID_Y-1);
	int baseTwoX = get_random(GRID_X-1);
	int baseTwoY = get_random(GRID_Y-1);
	while(getDistance(baseOneX, baseOneY, baseTwoX, baseTwoY) < ((GRID_X + GRID_Y)/2)){
		baseTwoX = get_random(GRID_X-1);
		baseTwoY = get_random(GRID_Y-1);
	}
	
	gridTerrain[baseOneX][baseOneY] = BASE;
	gridTerrain[baseTwoX][baseTwoY] = BASE;
	tileFaction[baseOneX][baseOneY] = FIRST;
	tileFaction[baseTwoX][baseTwoY] = SECOND;
	
	playerOneSelectX = baseOneX;
	playerOneSelectY = baseOneY;
	
	playerTwoSelectX = baseTwoX;
	playerTwoSelectY = baseTwoY;
	
	isPlayerOneSelecting = false;
	isPlayerTwoSelecting = false;
}

void doGameTick(){
	////printf("Current Tick: %d / %d\n", currentCalculatedTick, currentTrueTick);
	
	if(gameEnded == true) return;
	
	if(currentCalculatedTick < currentTrueTick){
	
		currentCalculatedTick += 1;	//do calculation if not keeping up yet

		if(currentCalculatedTick % 4 == 0){
			for(int i = 0; i < GRID_X; i++){
				for(int j = 0; j < GRID_Y; j++){
					if(tileFaction[i][j] != NONE){
						if(gridTerrain[i][j] == BASE || gridTerrain[i][j] == TOWER){
							if(unitCount[i][j] < MAX_UNIT){
								unitCount[i][j] += 1;	
							}
						}
					}
				}
			}
		}
	}
}

void doAnimation(){
	for(int i = 0; i < GRID_X; i++){
		for(int j = 0; j < GRID_Y; j++){
			if(animatedUnitCount[i][j] != unitCount[i][j]){
				if(animatedUnitCount[i][j] > unitCount[i][j]){
					animatedUnitCount[i][j] -= (((animatedUnitCount[i][j] - unitCount[i][j])/3 < 1) ? 1 : ((animatedUnitCount[i][j] - unitCount[i][j])/3));
				}else{
					animatedUnitCount[i][j] += (((unitCount[i][j] - animatedUnitCount[i][j])/3 < 1) ? 1 : ((unitCount[i][j] - animatedUnitCount[i][j])/3));
				}
			}
		}
	}
}

void tryMoveUnit(int side, int x, int y, int direction){
	if(gameEnded == true) return;
	if(side == FIRST && isPlayerOneSelecting != true) return;
	if(side == SECOND && isPlayerTwoSelecting != true) return;
	if(tileFaction[x][y] != side) return;
	if(unitCount[x][y] < 2) return;
	
	int unitToMove = 0;
	
	if(side == FIRST && playerOnePressShift == true){
		unitToMove = unitCount[x][y]/2;
	}else if(side == SECOND && playerTwoPressShift == true){
		unitToMove = unitCount[x][y]/2;
	}else{
		unitToMove = unitCount[x][y] - 1;
	}
	int unitRemain = unitCount[x][y] - unitToMove;
	
	int targetX = -1;
	int targetY = -1;
	
	switch(direction){
		case UP:
			if(y > 0){
				targetY = y - 1;
				targetX = x;
			}
			break;
		case DOWN:
			if(y < GRID_Y - 1){
				targetY = y + 1;
				targetX = x;
			}
			break;
		case LEFT:
			if(x > 0){
				targetY = y;
				targetX = x - 1;
			}
			break;
		case RIGHT:
			if(x < GRID_X - 1){
				targetY = y;
				targetX = x + 1;
			}
			break;
	}
	
	if(targetX == -1 || targetY == -1) return;
	if(gridTerrain[targetX][targetY] == MOUNTAIN) return;
	int anythingDone = false;
	if(tileFaction[targetX][targetY] == side){
		if((unitToMove + unitCount[targetX][targetY]) > MAX_UNIT){
			unitRemain += ((unitToMove + unitCount[targetX][targetY]) - MAX_UNIT);
			unitCount[targetX][targetY] = MAX_UNIT;
			unitCount[x][y] = unitRemain;
		}else{
			unitCount[targetX][targetY] += unitToMove;
			unitCount[x][y] = unitRemain;
		}
		anythingDone = true;
	}else if(tileFaction[targetX][targetY] == NONE){
		tileFaction[targetX][targetY] = side;
		unitCount[targetX][targetY] = unitToMove;
		unitCount[x][y] = unitRemain;
		anythingDone = true;
	}else{
		unitCount[targetX][targetY] -= unitToMove;
		unitCount[x][y] = unitRemain;
		if(unitCount[targetX][targetY] == 0){
			tileFaction[targetX][targetY] = NONE;
		}else if(unitCount[targetX][targetY] < 0){
			tileFaction[targetX][targetY] = side;
			unitCount[targetX][targetY] = (0 - unitCount[targetX][targetY]);
		}
		anythingDone = true;
		if(gridTerrain[targetX][targetY] == BASE){
			if(tileFaction[targetX][targetY] == side){
				showWinningSide(side);
			}
		}
	}
	
	if(anythingDone){
		if(side == FIRST){
			isPlayerOneSelecting = false;
			playerOneSelectX = targetX;
			playerOneSelectY = targetY;
		}else if(side == SECOND){
			isPlayerTwoSelecting = false;
			playerTwoSelectX = targetX;
			playerTwoSelectY = targetY;
		}
	}
}

int getMoveDirection(int keyCode){
	switch(keyCode){
		case 0x1D:
		case 0x75:
			return UP;
		case 0x1C:
		case 0x6B:
			return LEFT;
		case 0x23:
		case 0x74:
			return RIGHT;
		case 0x1B:
		case 0x72:
			return DOWN;
	}
	return NONE;
}

/* ↑↑↑ Game Logic ↑↑↑ */
/* ↓↓↓ Audio ↓↓↓ */
//note: due to the sim speed problem of CPULator,
//audio cant be used for now
/*
#define SAMPLE_RATE 8000.0
#define pi  3.141592
	
int currentFreq = 0;
double curDuration = 0;
int currentNote = 0;

int currentSampleCount = 0;	//current location through one full wave

#define SONG_LENGTH 3

int songNote[SONG_LENGTH] = {1000, 800, 1200};
double noteDuration[SONG_LENGTH] = {1, 1, 1};

void writeAudio(double freq, int samplesToGenerate){
	
	volatile int * AudioAddress = (int *) 0xff203040;
	
	int vol = 0x0FFFFFFF;
	
	for(int i = currentSampleCount; i < currentSampleCount+samplesToGenerate; i++){
		*(int *)(AudioAddress + 2) = vol * sin(2*pi*i*freq/SAMPLE_RATE);
		*(int *)(AudioAddress + 3) = vol * sin(2*pi*i*freq/SAMPLE_RATE);
	}
	currentSampleCount += samplesToGenerate;
	if(currentSampleCount > SAMPLE_RATE){
		currentSampleCount -= SAMPLE_RATE;	
	}
}

void generateChipTune(){
	unsigned int fifospace;
	volatile int * audio_ptr = (int *) 0xFF203040; // audio port
	fifospace = *(audio_ptr+1);
	while((fifospace & 0x00FF0000) > 0){
		if(curDuration<=0){
			currentNote+=1;
			if(currentNote >= SONG_LENGTH){
				currentNote = 0;
			}
			currentFreq = songNote[currentNote];
			curDuration = noteDuration[currentNote];
		}
		int spaceAvailable = ((fifospace & 0x00FF0000) >> 16);
		writeAudio(currentFreq, spaceAvailable);
		curDuration -= (spaceAvailable/SAMPLE_RATE);
		fifospace = *(audio_ptr+1);
	}
}*/
/* ↑↑↑ Audio ↑↑↑ */
/* ↓↓↓ Rendering ↓↓↓ */
void doRender(){
	doAnimation();
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	drawGrid();
	drawSelection(FIRST);
	drawSelection(SECOND);
	
	if(isPlayerOneSelecting){
		flashHighlight(FIRST);
	}
	if(isPlayerTwoSelecting){
		flashHighlight(SECOND);
	}
	wait_sync();
	pixel_buffer_start = *(pixel_ctrl_ptr + 1);
}

void renderText(int x, int y, char* text){
	while(*text){
		drawAscii(x,y,*text);
		text += 1;
		x += 1;
	}
}

void showWinningSide(int side){
	char* winText;
	if(side == FIRST){
		winText = "Blue Side Had Won";
	}else{
		winText = "Red Side Had Won";
	}
	int y = 58;
	int x = 2;
	renderText(x, y, winText);
	winText = "Press button 0 to reset";
	y = 59;
	x = 2;
	renderText(x, y, winText);
	gameEnded = true;
}

void drawHelp(){
	int gridEnd = (GRID_Y+1) * LINE_PER_GRID;
	renderText(5, gridEnd + 1, "Player 1: WASD = move cursor, Space = Select");
	renderText(5, gridEnd + 2, "          Hold shift when moving unit to move half instead of all");
	renderText(5, gridEnd + 3, "Player 2: Arrow keys = move cursor, Enter = Select");
	renderText(5, gridEnd + 4, "          Hold ctrl when moving unit to move half instead of all");
}

void drawUnitCount(int gridX, int gridY){
	int unit = animatedUnitCount[gridX][gridY];
	
	int currentY = (gridY + GRID_START_OFFSET_Y) * LINE_PER_GRID + 1;
	int currentX = (gridX + GRID_START_OFFSET_X) * GRID_TEXT_SIZE - 1;
	int counter = GRID_TEXT_SIZE - 1;
	while(unit){
		drawDigit(currentX + counter, currentY, unit % 10);
		unit /= 10;
		counter -= 1;
	}
	if(animatedUnitCount[gridX][gridY] > 0 && animatedUnitCount[gridX][gridY] < 10){
		drawDigit(currentX + counter, currentY, 0);
		//dirty fix..
	}
}

void drawTileType(int gridX, int gridY){
	int terrain = gridTerrain[gridX][gridY];
	int currentY = (gridY + GRID_START_OFFSET_Y) * LINE_PER_GRID + 2;
	int currentX = (gridX + GRID_START_OFFSET_X) * GRID_TEXT_SIZE + 2;
	switch(terrain){
		case EMPTY:
			break;
		case MOUNTAIN:
			drawAscii(currentX, currentY, 'M');
			break;
		case BASE:
			drawAscii(currentX, currentY, 'B');
			break;
		case TOWER:
			drawAscii(currentX, currentY, 'T');
			break;
	}
}

void drawTileFaction(int gridX, int gridY){
	int faction = tileFaction[gridX][gridY];
	int color = -1;
	if(faction == FIRST){
		color = FIRST_COLOR;
	}else if(faction == SECOND){
		color = SECOND_COLOR;
	}
	
	if(color != -1){
		int left = (GRID_START_OFFSET_X + gridX)*GRID_SIZE_X + 1;
		int right = (GRID_START_OFFSET_X + gridX + 1)*GRID_SIZE_X - 1;
		int top = (GRID_START_OFFSET_Y + gridY)*GRID_SIZE_Y + 1;
		int bottom = (GRID_START_OFFSET_Y + gridY + 1)*GRID_SIZE_Y - 1;
		
		draw_line(left, top, left, bottom, color);
		draw_line(right, top, right, bottom, color);
		draw_line(left, top, right, top, color);
		draw_line(left, bottom, right, bottom, color);
	}
}

void drawSelection(int side){
	int x;
	int y;
	if(side == FIRST){
		x = playerOneSelectX;
		y = playerOneSelectY;
	}else if(side == SECOND){
		x = playerTwoSelectX;
		y = playerTwoSelectY;
	}else{
		return;	
	}
	drawHighlight(x, y, side);
}

void flashHighlight(int side){
	if(currentCalculatedTick % 4 < 2){
		int x;
		int y;
		if(side == FIRST){
			x = playerOneSelectX;
			y = playerOneSelectY;
		}else if(side == SECOND){
			x = playerTwoSelectX;
			y = playerTwoSelectY;
		}else{
			return;	
		}
		if(x > 0 && gridTerrain[x - 1][y] != MOUNTAIN){
			drawHighlight(x - 1, y, side);
		}
		if(x < GRID_X - 1 && gridTerrain[x + 1][y] != MOUNTAIN){
			drawHighlight(x + 1, y, side);
		}
		if(y > 0 && gridTerrain[x][y - 1] != MOUNTAIN){
			drawHighlight(x, y - 1, side);
		}
		if(y < GRID_Y - 1 && gridTerrain[x][y+1] != MOUNTAIN){
			drawHighlight(x, y + 1, side);
		}
	}
}

void drawHighlight(int x, int y, int side){
	int color;
	if(side == FIRST){
		color = FIRST_SELECT_COLOR;
	}else if(side == SECOND){
		color = SECOND_SELECT_COLOR;
	}else{
		return;	
	}
	
	
	int left = (GRID_START_OFFSET_X + x)*GRID_SIZE_X;
	int right = (GRID_START_OFFSET_X + x + 1)*GRID_SIZE_X;
	int top = (GRID_START_OFFSET_Y + y)*GRID_SIZE_Y;
	int bottom = (GRID_START_OFFSET_Y + y + 1)*GRID_SIZE_Y;

	draw_line(left, top, left, bottom, color);
	draw_line(right, top, right, bottom, color);
	draw_line(left, top, right, top, color);
	draw_line(left, bottom, right, bottom, color);
}

void drawGrid(){
	//first draw vertical lines
	for(int i = GRID_START_OFFSET_X; i <= GRID_START_OFFSET_X+GRID_X; i++){
		draw_line(GRID_SIZE_X * i, GRID_START_Y, GRID_SIZE_X * i, GRID_END_Y, GRID_COLOR);
	}
	//then draw horizontal lines
	for(int i = GRID_START_OFFSET_Y; i <= GRID_START_OFFSET_Y+GRID_Y; i++){
		draw_line(GRID_START_X, GRID_SIZE_Y * i,GRID_END_X, GRID_SIZE_Y * i, GRID_COLOR);
	}
	
	//then for all tile draw its unit count
	for(int i = 0; i < GRID_X; i++){
		for(int j = 0; j < GRID_Y; j++){
			drawUnitCount(i, j);
			drawTileType(i, j);
			drawTileFaction(i, j);
		}
	}
}

//used to initialize the pixel buffer
void initializeBuffer(){
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	
	*(pixel_ctrl_ptr + 1) = 0xC8000000;
	wait_sync();	//then switch this to the current (so the other will will be swapped to back buffer and we can change its location
	pixel_buffer_start = *(pixel_ctrl_ptr);
	clear_screen();

	*(pixel_ctrl_ptr + 1) = 0xC0000000;		//then we set the back buffer to another address
	pixel_buffer_start = *(pixel_ctrl_ptr + 1);
	clear_screen();
}


void swap(int *xp, int *yp){
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}


void draw_line(int x0, int y0, int x1, int y1, short int color){
	int is_steep = (ABS(y1 - y0) > ABS(x1 - x0)) ? 1 : 0;
	
	if(is_steep == 1){
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	
	if (x0 > x1){
		swap(&x0, &x1);
		swap(&y0, &y1);
	}
	
	int deltaX = x1 - x0;
	int deltaY = ABS(y1 - y0);
	int error = -(deltaX / 2);
	int y = y0;
	int y_step = (y0 < y1) ? 1 : -1;
	
	
	for(int x = x0; x <= x1; x++){
		
		if(is_steep == 1){
			plot_pixel(y, x, color);
		}else{
			plot_pixel(x, y, color);
		}
		
		error += deltaY;
		
		if(error > 0){
			y += y_step;
			error -= deltaX;
		}
	}
	
}

void wait_sync(){
	volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	*pixel_ctrl_ptr = 1;		//enable sync
	volatile char * status_ptr = (char *)0xFF20302C;
	while(1){
		if(((*status_ptr) & 1) == 0) break;	//wait for status to be 0
	}
}

void clear_screen(){
	volatile short int * pixel_ctrl_ptr = (short int *)0xFF203020;
	short x = *(pixel_ctrl_ptr+4);
	short y = *(pixel_ctrl_ptr+5);
	for(int currentX = 0; currentX < x; currentX++){
		for(int currentY = 0; currentY < y; currentY++){
			plot_pixel(currentX, currentY, BACKGROUND_COLOR);
		}
	}
	for(int currentX = 0; currentX < 80; currentX++){
		for(int currentY = 0; currentY < 60; currentY++){
			drawAscii(currentX, currentY, ' ');
		}
	}
}

void plot_pixel(int x, int y, short int line_color)
{	
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

void drawDigit(int x, int y, int val){
	drawAscii(x, y, '0'+val);
}

void drawAscii(int x, int y, char val){
	volatile char * character_buffer = (char *) (0xC9000000 + (y<<7) + x);
	*character_buffer = val;
}
/* ↑↑↑ Rendering ↑↑↑ */
