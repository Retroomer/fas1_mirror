
#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>
#include <string.h>

static const unsigned char nintendo_logo[] =
{
	0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD,
	0x11,0x24,0x8B,0x98,0xC0,0x81,0x7F,0x21,0xA3,0x52,0xBE,0x19,0x93,0x09,0xCE,0x20,
	0x10,0x46,0x4A,0x4A,0xF8,0x27,0x31,0xEC,0x58,0xC7,0xE8,0x33,0x82,0xE3,0xCE,0xBF,
	0x85,0xF4,0xDF,0x94,0xCE,0x4B,0x09,0xC1,0x94,0x56,0x8A,0xC0,0x13,0x72,0xA7,0xFC,
	0x9F,0x84,0x4D,0x73,0xA3,0xCA,0x9A,0x61,0x58,0x97,0xA3,0x27,0xFC,0x03,0x98,0x76,
	0x23,0x1D,0xC7,0x61,0x03,0x04,0xAE,0x56,0xBF,0x38,0x84,0x00,0x40,0xA7,0x0E,0xFD,
	0xFF,0x52,0xFE,0x03,0x6F,0x95,0x30,0xF1,0x97,0xFB,0xC0,0x85,0x60,0xD6,0x80,0x25,
	0xA9,0x63,0xBE,0x03,0x01,0x4E,0x38,0xE2,0xF9,0xA2,0x34,0xFF,0xBB,0x3E,0x03,0x44,
	0x78,0x00,0x90,0xCB,0x88,0x11,0x3A,0x94,0x65,0xC0,0x7C,0x63,0x87,0xF0,0x3C,0xAF,
	0xD6,0x25,0xE4,0x8B,0x38,0x0A,0xAC,0x72,0x21,0xD4,0xF8,0x07,
};

struct BootItem
{
	char title[12];
	char* address;
};

#define MAX_ITEMS 18
BootItem items[MAX_ITEMS];
int itemCount = 0;
int selectedItem = 0;


void ScanItems()
{
	char* ptr = (char*)0x8000000;
	itemCount = 0;
	while(itemCount < MAX_ITEMS && ptr < (char*)0xA000000)
	{
		bool isds = false;
		bool isgba = false;
		if(strncmp("PASS", ptr+0xAC, 4) == 0)
		{
			isds = true;
		}

		if(memcmp(nintendo_logo, ptr+0x4, sizeof(nintendo_logo)) == 0)
		{
			isgba = true;
		}
		
		if(isgba && !isds)
		{
			strncpy(items[itemCount].title, ptr+0xA0, 12);
			items[itemCount].title[12] = '\0';
			items[itemCount].address = ptr - 0x8000000;
			itemCount++;
		}

		ptr += 0x40000;
	}
}

void PrintItems()
{
	// clear screen
	iprintf("\e[2JBoot\n");
	
	for(int i = 0; i < itemCount; i++)
	{
		iprintf(
			"\n%c %12s (%08x)",
			(i == selectedItem) ? '*' : ' ',
			items[i].title,
			items[i].address);
	}
}

void WriteRepeat(u32 addr, u16 data, u16 count)
{
	u16 i;
	for (i=0; i<count; i++)
	{
		*(vu16 *)(0x8000000 + (addr << 1)) = data;
	}
}

void VisolyModePreamble()
{
	WriteRepeat (0x987654, 0x5354, 1);
	WriteRepeat ( 0x12345, 0x1234, 500);
	WriteRepeat (  0x7654, 0x5354, 1);
	WriteRepeat ( 0x12345, 0x5354, 1);
	WriteRepeat ( 0x12345, 0x5678, 500);
	WriteRepeat (0x987654, 0x5354, 1);
	WriteRepeat ( 0x12345, 0x5354, 1);
	WriteRepeat (0x765400, 0x5678, 1);
	WriteRepeat ( 0x13450, 0x1234, 1);
	WriteRepeat ( 0x12345, 0xabcd, 500);
	WriteRepeat (0x987654, 0x5354, 1);
}

void VisolySetFlashBaseAddress(u32 offset)
{
	u16 base = ((offset >> 22) & 3) | ((offset >> 22) & 4) | ((offset >> 12) & 0x3F8);
	VisolyModePreamble();
	WriteRepeat(0xB5AC97, base, 1);
}

void Boot()
{
	VisolySetFlashBaseAddress((u32)items[selectedItem].address);
	//SoftReset(ROM_RESTART);
	
	__asm volatile (
		"swi 0x26\n"
	:
	:
	:
	);
}

int main(void)
{
	// the vblank interrupt must be enabled for VBlankIntrWait() to work
	// since the default dispatcher handles the bios flags no vblank handler
	// is required
	irqInit();
	irqEnable(IRQ_VBLANK);

	consoleInit( 0 , 4 , 0, NULL , 0 , 15);

	BG_COLORS[0]=RGB8(33,171,243);
	BG_COLORS[241]=RGB5(31,31,31);

	SetMode(MODE_0 | BG0_ON);
	
	VisolySetFlashBaseAddress(0);
	ScanItems();
	if(itemCount == 0)
	{
		iprintf("No programs found\n");
		for(;;);
	}
	
	PrintItems();
	
	while (1)
	{
		VBlankIntrWait();
		scanKeys();
		int keys = keysDown();
		if(keys & KEY_UP)
		{
			selectedItem--;
			selectedItem = (selectedItem < 0) ? 0 : selectedItem;
			PrintItems();
		}
		if(keys & KEY_DOWN)
		{
			selectedItem++;
			selectedItem = (selectedItem >= itemCount) ? itemCount-1 : selectedItem;
			PrintItems();
		}
		if(keys & KEY_A)
		{
			Boot();
		}
	}
}


