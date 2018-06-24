#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/tpl.h>

#include "textures_tpl.h"
#include "textures.h"

#define DEFAULT_FIFO_SIZE	(256*1024)

static void *frameBuffer[2] = { NULL, NULL};
static GXRModeObj *rmode;

#define WIDTH 640
#define HEIGHT 480

#define CELL_WIDTH 24
#define CELL_HEIGHT 20
#define UNIVERSE_SIZE CELL_HEIGHT * CELL_WIDTH
#define CELL_SIZE 20

typedef struct {
	int x,y;
	int image;
}Sprite;

Sprite sprites[UNIVERSE_SIZE];

GXTexObj texObj;

void drawSpriteTex( int x, int y, int width, int height, int image );

int main( int argc, char **argv ){
	u32	fb;
	u32 first_frame;
	f32 yscale;
	u32 xfbHeight;
	Mtx44 perspective;
	Mtx model;
	void *gp_fifo = NULL;

	GXColor background = {0x22, 0x22, 0x22, 0xff};

	int i;

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);

	fb = 0;
	first_frame = 1;
	
	frameBuffer[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	frameBuffer[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(frameBuffer[fb]);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	fb ^= 1;

	// setup the fifo and then init the flipper
	gp_fifo = memalign(32,DEFAULT_FIFO_SIZE);
	memset(gp_fifo,0,DEFAULT_FIFO_SIZE);

	GX_Init(gp_fifo,DEFAULT_FIFO_SIZE);

	// clears the bg to color and clears the z buffer
	GX_SetCopyClear(background, 0x00ffffff);

	// other gx setup
	GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
	yscale = GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
	xfbHeight = GX_SetDispCopyYScale(yscale);
	GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
	GX_SetDispCopyDst(rmode->fbWidth,xfbHeight);
	GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
	GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));

	/*if (rmode->aa)
		GX_SetPixelFmt(GX_PF_RGB565_Z16, GX_ZC_LINEAR);
	else
		GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);*/


	GX_SetCullMode(GX_CULL_NONE);
	GX_CopyDisp(frameBuffer[fb],GX_TRUE);
	GX_SetDispCopyGamma(GX_GM_1_0);

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);


	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);


	GX_InvalidateTexAll();

	TPLFile spriteTPL;
	TPL_OpenTPLFromMemory(&spriteTPL, (void *)textures_tpl,textures_tpl_size);
	TPL_GetTexture(&spriteTPL,ballsprites,&texObj);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);

	guOrtho(perspective,0,479,0,639,0,300);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	WPAD_Init();

	srand(time(NULL));
	
	int xMargin = ((WIDTH - ((CELL_WIDTH + 1) * CELL_SIZE)) / 2) + (CELL_SIZE / 2);
	int yMargin = ((HEIGHT - ((CELL_HEIGHT + 1 ) * CELL_SIZE)) / 2) + (CELL_SIZE / 3 * 2);

	for(int x = 0; x <= CELL_WIDTH; x++) {
		for(int y = 0; y <= CELL_HEIGHT; y++) {
			
		int offset = (x * CELL_HEIGHT) + y;
		//random place and speed
		sprites[offset].x = (x * CELL_SIZE) + xMargin;
		sprites[offset].y = (y * CELL_SIZE) + yMargin;
		sprites[offset].image = rand() & 3;

		}
	}

	while(1) {

		WPAD_ScanPads();

		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) exit(0);

		GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
		GX_InvVtxCache();
		GX_InvalidateTexAll();

		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

		guMtxIdentity(model);
		guMtxTransApply (model, model, 0.0F, 0.0F, -5.0F);
		GX_LoadPosMtxImm(model,GX_PNMTX0);

		for(i = 0; i < UNIVERSE_SIZE; i++) {
			drawSpriteTex( sprites[i].x, sprites[i].y, CELL_SIZE, CELL_SIZE, sprites[i].image);
		}
		
		
		GX_DrawDone();


		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetColorUpdate(GX_TRUE);
		GX_CopyDisp(frameBuffer[fb],GX_TRUE);
		

		VIDEO_SetNextFramebuffer(frameBuffer[fb]);
		if(first_frame) {
			VIDEO_SetBlack(FALSE);
			first_frame = 0;
		}
		VIDEO_Flush();
		VIDEO_WaitVSync();
		fb ^= 1;		// flip framebuffer
	}
	return 0;
}

float texCoords[] = {
	0.0 ,0.0 , 0.5, 0.0, 0.5, 0.5, 0.0, 0.5,
	0.5 ,0.0 , 1.0, 0.0, 1.0, 0.5, 0.5, 0.5,
	0.0 ,0.5 , 0.5, 0.5, 0.5, 1.0, 0.0, 1.0,
	0.5 ,0.5 , 1.0, 0.5, 1.0, 1.0, 0.5, 1.0
};

void drawSpriteTex( int x, int y, int width, int height, int image ) {

	int texIndex = image * 8;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			
		GX_Position2f32(x, y);					// Top Left
		//GX_Color3f32(1.0f, 0.5f, 0.5f); 
		GX_TexCoord2f32(texCoords[texIndex],texCoords[texIndex+1]);
		texIndex+=2;
		GX_Position2f32((x+width-1), y);			// Top Right
		//GX_Color3f32(1.0f, 0.5f, 0.5f); 
		GX_TexCoord2f32(texCoords[texIndex],texCoords[texIndex+1]);
		texIndex+=2;
		GX_Position2f32((x+width-1),y+height-1);	// Bottom Right
		//GX_Color3f32(1.0f, 0.5f, 0.5f); 
		GX_TexCoord2f32(texCoords[texIndex],texCoords[texIndex+1]);
		texIndex+=2;
		GX_Position2f32(x,y+height-1);			// Bottom Left
		//GX_Color3f32(1.0f, 0.5f, 0.5f); 
		GX_TexCoord2f32(texCoords[texIndex],texCoords[texIndex+1]);
	GX_End();									

}

