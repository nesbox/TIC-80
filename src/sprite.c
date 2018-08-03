// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "sprite.h"
#include "history.h"

#define CANVAS_SIZE (64)
#define PALETTE_CELL_SIZE 8
#define PALETTE_ROWS 2
#define PALETTE_COLS (TIC_PALETTE_SIZE / PALETTE_ROWS)
#define PALETTE_WIDTH (PALETTE_COLS * PALETTE_CELL_SIZE)
#define PALETTE_HEIGHT (PALETTE_ROWS * PALETTE_CELL_SIZE)
#define SHEET_COLS (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE)

static void clearCanvasSelection(Sprite* sprite)
{
	memset(&sprite->select.rect, 0, sizeof(tic_rect));
}

static u8 getSheetPixel(Sprite* sprite, s32 x, s32 y)
{
	return getSpritePixel(sprite->src->data, x, sprite->index >= TIC_BANK_SPRITES ? y + TIC_SPRITESHEET_SIZE: y);
}

static void setSheetPixel(Sprite* sprite, s32 x, s32 y, u8 color)
{
	setSpritePixel(sprite->src->data, x, sprite->index >= TIC_BANK_SPRITES ? y + TIC_SPRITESHEET_SIZE: y, color);
}

static s32 getIndexPosX(Sprite* sprite)
{
	s32 index = sprite->index % TIC_BANK_SPRITES;
	return index % SHEET_COLS * TIC_SPRITESIZE;
}

static s32 getIndexPosY(Sprite* sprite)
{
	s32 index = sprite->index % TIC_BANK_SPRITES;
	return index / SHEET_COLS * TIC_SPRITESIZE;
}

static void drawSelection(Sprite* sprite, s32 x, s32 y, s32 w, s32 h)
{
	enum{Step = 3};
	u8 color = (tic_color_white);

	s32 index = sprite->tickCounter / 10;
	for(s32 i = x; i < (x+w); i++) 		{ sprite->tic->api.pixel(sprite->tic, i, y, index++ % Step ? color : 0);} index++;
	for(s32 i = y; i < (y+h); i++) 		{ sprite->tic->api.pixel(sprite->tic, x + w-1, i, index++ % Step ? color : 0);} index++;
	for(s32 i = (x+w-1); i >= x; i--) 	{ sprite->tic->api.pixel(sprite->tic, i, y + h-1, index++ % Step ? color : 0);} index++;
	for(s32 i = (y+h-1); i >= y; i--) 	{ sprite->tic->api.pixel(sprite->tic, x, i, index++ % Step ? color : 0);}
}

static tic_rect getSpriteRect(Sprite* sprite)
{
	s32 x = getIndexPosX(sprite);
	s32 y = getIndexPosY(sprite);

	return (tic_rect){x, y, sprite->size, sprite->size};
}

static void drawCursorBorder(Sprite* sprite, s32 x, s32 y, s32 w, s32 h)
{
	sprite->tic->api.rect_border(sprite->tic, x, y, w, h, (tic_color_black));
	sprite->tic->api.rect_border(sprite->tic, x-1, y-1, w+2, h+2, (tic_color_white));
}

static void processPickerCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 sx, s32 sy)
{
	tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
	const s32 Size = CANVAS_SIZE / sprite->size;

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;

		mx -= mx % Size;
		my -= my % Size;

		drawCursorBorder(sprite, x + mx, y + my, Size, Size);

		if(checkMouseDown(&rect, tic_mouse_left))
			sprite->color = getSheetPixel(sprite, sx + mx / Size, sy + my / Size);

		if(checkMouseDown(&rect, tic_mouse_right))
			sprite->color2 = getSheetPixel(sprite, sx + mx / Size, sy + my / Size);
	}
}

static void processDrawCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 sx, s32 sy)
{
	tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
	const s32 Size = CANVAS_SIZE / sprite->size;

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;


		s32 brushSize = sprite->brushSize*Size;
		s32 offset = (brushSize - Size) / 2;

		mx -= offset;
		my -= offset;
		mx -= mx % Size;
		my -= my % Size;

		if(mx < 0) mx = 0;
		if(my < 0) my = 0;
		if(mx+brushSize >= CANVAS_SIZE) mx = CANVAS_SIZE - brushSize;
		if(my+brushSize >= CANVAS_SIZE) my = CANVAS_SIZE - brushSize;

		drawCursorBorder(sprite, x + mx, y + my, brushSize, brushSize);

		bool left = checkMouseDown(&rect, tic_mouse_left);
		bool right = checkMouseDown(&rect, tic_mouse_right);

		if(left || right)
		{
			sx += mx / Size;
			sy += my / Size;
			u8 color = left ? sprite->color : sprite->color2;
			s32 pixels = sprite->brushSize;

			for(s32 j = 0; j < pixels; j++)
				for(s32 i = 0; i < pixels; i++)
					setSheetPixel(sprite, sx+i, sy+j, color);

			history_add(sprite->history);
		}
	}
}

static void pasteSelection(Sprite* sprite)
{
	s32 l = getIndexPosX(sprite);
	s32 t = getIndexPosY(sprite);
	s32 r = l + sprite->size;
	s32 b = t + sprite->size;

	for(s32 sy = t, i = 0; sy < b; sy++)
		for(s32 sx = l; sx < r; sx++)
			setSheetPixel(sprite, sx, sy, sprite->select.back[i++]);

	tic_rect* rect = &sprite->select.rect;

	l += rect->x;
	t += rect->y;
	r = l + rect->w;
	b = t + rect->h;

	for(s32 sy = t, i = 0; sy < b; sy++)
		for(s32 sx = l; sx < r; sx++)
			setSheetPixel(sprite, sx, sy, sprite->select.front[i++]);

	history_add(sprite->history);
}

static void copySelection(Sprite* sprite)
{
	tic_rect rect = getSpriteRect(sprite);
	s32 r = rect.x + rect.w;
	s32 b = rect.y + rect.h;

	for(s32 sy = rect.y, i = 0; sy < b; sy++)
		for(s32 sx = rect.x; sx < r; sx++)
			sprite->select.back[i++] = getSheetPixel(sprite, sx, sy);

	{
		tic_rect* rect = &sprite->select.rect;
		memset(sprite->select.front, 0, CANVAS_SIZE * CANVAS_SIZE);

		for(s32 j = rect->y, index = 0; j < (rect->y + rect->h); j++)
			for(s32 i = rect->x; i < (rect->x + rect->w); i++)
			{
				u8* color = &sprite->select.back[i+j*sprite->size];
				sprite->select.front[index++] = *color;
				*color = sprite->color2;
			}
	}
}

static void processSelectCanvasMouse(Sprite* sprite, s32 x, s32 y)
{
	tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
	const s32 Size = CANVAS_SIZE / sprite->size;

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;

		mx -= mx % Size;
		my -= my % Size;

		drawCursorBorder(sprite, x + mx, y + my, Size, Size);

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			if(sprite->select.drag)
			{
				s32 x = mx / Size;
				s32 y = my / Size;

				s32 rl = MIN(x, sprite->select.start.x);
				s32 rt = MIN(y, sprite->select.start.y);
				s32 rr = MAX(x, sprite->select.start.x);
				s32 rb = MAX(y, sprite->select.start.y);

				sprite->select.rect = (tic_rect){rl, rt, rr - rl + 1, rb - rt + 1};
			}
			else
			{
				sprite->select.drag = true;
				sprite->select.start = (tic_point){mx / Size, my / Size};
				sprite->select.rect = (tic_rect){sprite->select.start.x, sprite->select.start.y, 1, 1};
			}
		}
		else if(sprite->select.drag)
		{
			copySelection(sprite);
			sprite->select.drag = false;
		}
	}
}

static void floodFill(Sprite* sprite, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y, u8 color, u8 fill)
{
	if(getSheetPixel(sprite, x, y) == color)
	{
		setSheetPixel(sprite, x, y, fill);

		if(x > l) floodFill(sprite, l, t, r, b, x-1, y, color, fill);
		if(x < r) floodFill(sprite, l, t, r, b, x+1, y, color, fill);
		if(y > t) floodFill(sprite, l, t, r, b, x, y-1, color, fill);
		if(y < b) floodFill(sprite, l, t, r, b, x, y+1, color, fill);
	}
}

static void replaceColor(Sprite* sprite, s32 l, s32 t, s32 r, s32 b, s32 x, s32 y, u8 color, u8 fill)
{
	for(s32 sy = t; sy < b; sy++)
		for(s32 sx = l; sx < r; sx++)
			if(getSheetPixel(sprite, sx, sy) == color)
				setSheetPixel(sprite, sx, sy, fill);
}

static void processFillCanvasMouse(Sprite* sprite, s32 x, s32 y, s32 l, s32 t)
{
	tic_mem* tic = sprite->tic;
	tic_rect rect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
	const s32 Size = CANVAS_SIZE / sprite->size;

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;

		mx -= mx % Size;
		my -= my % Size;

		drawCursorBorder(sprite, x + mx, y + my, Size, Size);

		bool left = checkMouseClick(&rect, tic_mouse_left);
		bool right = checkMouseClick(&rect, tic_mouse_right);

		if(left || right)
		{
			s32 sx = l + mx / Size;
			s32 sy = t + my / Size;

			u8 color = getSheetPixel(sprite, sx, sy);
			u8 fill = left ? sprite->color : sprite->color2;

			if(color != fill)
			{
				tic->api.key(tic, tic_key_ctrl)
					? replaceColor(sprite, l, t, l + sprite->size-1, t + sprite->size-1, sx, sy, color, fill)
					: floodFill(sprite, l, t, l + sprite->size-1, t + sprite->size-1, sx, sy, color, fill);
			}

			history_add(sprite->history);
		}
	}
}

static bool hasCanvasSelection(Sprite* sprite)
{
	return sprite->mode == SPRITE_SELECT_MODE && sprite->select.rect.w && sprite->select.rect.h;
}

static void drawBrushSlider(Sprite* sprite, s32 x, s32 y)
{
	enum {Count = 4, Size = 5};

	tic_rect rect = {x, y, Size, (Size+1)*Count};

	bool over = false;
	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		showTooltip("BRUSH SIZE");
		over = true;

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			s32 my = getMouseY() - y;

			sprite->brushSize = Count - my / (Size+1);
		}
	}

	sprite->tic->api.rect(sprite->tic, x+1, y, Size-2, Size*Count, (tic_color_black));

	for(s32 i = 0; i < Count; i++)
	{
		s32 offset = y + i*(Size+1);

		sprite->tic->api.rect(sprite->tic, x, offset, Size, Size, (tic_color_black));
		sprite->tic->api.rect(sprite->tic, x + 6, offset + 2, Count - i, 1, (tic_color_black));
	}

	sprite->tic->api.rect(sprite->tic, x+2, y+1, 1, Size*Count+1, (over ? tic_color_white : tic_color_gray));

	s32 offset = y + (Count - sprite->brushSize)*(Size+1);
	sprite->tic->api.rect(sprite->tic, x, offset, Size, Size, (tic_color_black));
	sprite->tic->api.rect(sprite->tic, x+1, offset+1, Size-2, Size-2, (over ? tic_color_white : tic_color_gray));
}

static void drawCanvas(Sprite* sprite, s32 x, s32 y)
{
	if(!hasCanvasSelection(sprite))
	{
		char buf[] = "#255";
		sprintf(buf, "#%03i", sprite->index);

		s32 ix = x + (CANVAS_SIZE - 4*TIC_FONT_WIDTH)/2;
		s32 iy = TIC_SPRITESIZE + 2;
		sprite->tic->api.text(sprite->tic, buf, ix, iy+1, (tic_color_black), false);
		sprite->tic->api.text(sprite->tic, buf, ix, iy, (tic_color_white), false);
	}

	sprite->tic->api.rect_border(sprite->tic, x-1, y-1, CANVAS_SIZE+2, CANVAS_SIZE+2, (tic_color_white));
	sprite->tic->api.rect(sprite->tic, x, y, CANVAS_SIZE, CANVAS_SIZE, (tic_color_black));
	sprite->tic->api.rect(sprite->tic, x-1, y + CANVAS_SIZE+1, CANVAS_SIZE+2, 1, (tic_color_black));

	if(!sprite->editPalette)
	{
		if(sprite->mode == SPRITE_DRAW_MODE)
		{
			drawBrushSlider(sprite, x - 15, y + 20);
		}		
	}
}

static void drawCanvasOvr(Sprite* sprite, s32 x, s32 y)
{
	tic_rect rect = getSpriteRect(sprite);
	s32 r = rect.x + rect.w;
	s32 b = rect.y + rect.h;

	const s32 Size = CANVAS_SIZE / sprite->size;

	for(s32 sy = rect.y, j = y; sy < b; sy++, j += Size)
		for(s32 sx = rect.x, i = x; sx < r; sx++, i += Size)
			sprite->tic->api.rect(sprite->tic, i, j, Size, Size, getSheetPixel(sprite, sx, sy));


	if(hasCanvasSelection(sprite))
		drawSelection(sprite, x + sprite->select.rect.x * Size - 1, y + sprite->select.rect.y * Size - 1, 
			sprite->select.rect.w * Size + 2, sprite->select.rect.h * Size + 2);

	if(!sprite->editPalette)
	{
		switch(sprite->mode)
		{
		case SPRITE_DRAW_MODE: 
			processDrawCanvasMouse(sprite, x, y, rect.x, rect.y);
			break;
		case SPRITE_PICK_MODE: processPickerCanvasMouse(sprite, x, y, rect.x, rect.y); break;
		case SPRITE_SELECT_MODE: processSelectCanvasMouse(sprite, x, y); break;
		case SPRITE_FILL_MODE: processFillCanvasMouse(sprite, x, y, rect.x, rect.y); break;
		}		
	}

	tic_rect canvasRect = {x, y, CANVAS_SIZE, CANVAS_SIZE};
	if(checkMouseDown(&canvasRect, tic_mouse_middle))
	{
		s32 mx = getMouseX() - x;
		s32 my = getMouseY() - y;
		sprite->color = getSheetPixel(sprite, rect.x + mx / Size, rect.y + my / Size);
	}
}

static void upCanvas(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	if(rect->y > 0) rect->y--;
	pasteSelection(sprite);
}

static void downCanvas(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	if(rect->y + rect->h < sprite->size) rect->y++;
	pasteSelection(sprite);
}

static void leftCanvas(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	if(rect->x > 0) rect->x--;
	pasteSelection(sprite);
}

static void rightCanvas(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	if(rect->x + rect->w < sprite->size) rect->x++;
	pasteSelection(sprite);
}

static void rotateSelectRect(Sprite* sprite)
{
	tic_rect rect = sprite->select.rect;
	
	s32 selection_center_x = rect.x + rect.w/2;
	s32 selection_center_y = rect.y + rect.h/2;
	
	// Rotate
	sprite->select.rect.w = rect.h;
	sprite->select.rect.h = rect.w;
	
	// Make the new center be at the position of the previous center
	sprite->select.rect.x -= (sprite->select.rect.x + sprite->select.rect.w/2) - selection_center_x;
	sprite->select.rect.y -= (sprite->select.rect.y + sprite->select.rect.h/2) - selection_center_y;
	
	// Check if we are not out of boundaries
	if (sprite->select.rect.x < 0) sprite->select.rect.x = 0;
	if (sprite->select.rect.y < 0) sprite->select.rect.y = 0;
	
	if (sprite->select.rect.x + sprite->select.rect.w >= sprite->size)
	{
		sprite->select.rect.x -= sprite->select.rect.x + sprite->select.rect.w - sprite->size;
	}
	
	if (sprite->select.rect.y + sprite->select.rect.h >= sprite->size)
	{
		sprite->select.rect.y -= sprite->select.rect.y + sprite->select.rect.h - sprite->size;
	}
}

static void rotateCanvas(Sprite* sprite)
{
	u8* buffer = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);

	if(buffer)
	{
		{
			tic_rect rect = sprite->select.rect;
			const s32 Size = rect.h * rect.w;
			s32 diff = 0;

			for(s32 y = 0, i = 0; y < rect.w; y++)
				for(s32 x = 0; x < rect.h; x++)
				{
					diff = rect.w * (x + 1) -y;
					buffer[i++] = sprite->select.front[Size - diff];
				}
			
			for (s32 i = 0; i<Size; i++)
				sprite->select.front[i] = buffer[i];
			
			rotateSelectRect(sprite);
			pasteSelection(sprite);
			history_add(sprite->history);
		}

		free(buffer);
	}
}

static void deleteCanvas(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	
	s32 left = getIndexPosX(sprite) + rect->x;
	s32 top = getIndexPosY(sprite) + rect->y;
	s32 right = left + rect->w;
	s32 bottom = top + rect->h;

	for(s32 pixel_y = top; pixel_y < bottom; pixel_y++)
		for(s32 pixel_x = left; pixel_x < right; pixel_x++)
			setSheetPixel(sprite, pixel_x, pixel_y, sprite->color2);

	clearCanvasSelection(sprite);
	
	history_add(sprite->history);
}

static void flipCanvasHorz(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	
	s32 sprite_x = getIndexPosX(sprite);
	s32 sprite_y = getIndexPosY(sprite);
	
	s32 right = sprite_x + rect->x + rect->w/2;
	s32 bottom = sprite_y + rect->y + rect->h;

	for(s32 y = sprite_y + rect->y; y < bottom; y++)
		for(s32 x = sprite_x + rect->x, i = sprite_x + rect->x + rect->w - 1; x < right; x++, i--)
		{
			u8 color = getSheetPixel(sprite, x, y);
			setSheetPixel(sprite, x, y, getSheetPixel(sprite, i, y));
			setSheetPixel(sprite, i, y, color);
		}

	history_add(sprite->history);
	copySelection(sprite);
}

static void flipCanvasVert(Sprite* sprite)
{
	tic_rect* rect = &sprite->select.rect;
	
	s32 sprite_x = getIndexPosX(sprite);
	s32 sprite_y = getIndexPosY(sprite);
	
	s32 right = sprite_x + rect->x + rect->w;
	s32 bottom = sprite_y + rect->y + rect->h/2;

	for(s32 y = sprite_y + rect->y, i = sprite_y + rect->y + rect->h - 1; y < bottom; y++, i--)
		for(s32 x = sprite_x + rect->x; x < right; x++)
		{
			u8 color = getSheetPixel(sprite, x, y);
			setSheetPixel(sprite, x, y, getSheetPixel(sprite, x, i));
			setSheetPixel(sprite, x, i, color);
		}

	history_add(sprite->history);
	copySelection(sprite);
}

static void drawMoveButtons(Sprite* sprite)
{
	if(hasCanvasSelection(sprite))
	{
		enum { x = 24 };
		enum { y = 20 };

		static const u8 Icons[] = 
		{
			0b00010000,
			0b00111000,
			0b01111100,
			0b11111110,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,

			0b11111110,
			0b01111100,
			0b00111000,
			0b00010000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,

			0b00010000,
			0b00110000,
			0b01110000,
			0b11110000,
			0b01110000,
			0b00110000,
			0b00010000,
			0b00000000,

			0b10000000,
			0b11000000,
			0b11100000,
			0b11110000,
			0b11100000,
			0b11000000,
			0b10000000,
			0b00000000,
		};

		static const tic_rect Rects[] = 
		{
			{x + (CANVAS_SIZE - TIC_SPRITESIZE)/2, y - TIC_SPRITESIZE, TIC_SPRITESIZE, TIC_SPRITESIZE/2},
			{x + (CANVAS_SIZE - TIC_SPRITESIZE)/2, y + CANVAS_SIZE + TIC_SPRITESIZE/2, TIC_SPRITESIZE, TIC_SPRITESIZE/2},
			{x - TIC_SPRITESIZE, y + (CANVAS_SIZE - TIC_SPRITESIZE)/2, TIC_SPRITESIZE/2, TIC_SPRITESIZE},
			{x + CANVAS_SIZE + TIC_SPRITESIZE/2, y + (CANVAS_SIZE - TIC_SPRITESIZE)/2, TIC_SPRITESIZE/2, TIC_SPRITESIZE},
		};

		static void(* const Func[])(Sprite*) = {upCanvas, downCanvas, leftCanvas, rightCanvas};

		bool down = false;
		for(s32 i = 0; i < sizeof Icons / 8; i++)
		{
			down = false;

			if(checkMousePos(&Rects[i]))
			{
				setCursor(tic_cursor_hand);

				if(checkMouseDown(&Rects[i], tic_mouse_left)) down = true;

				if(checkMouseClick(&Rects[i], tic_mouse_left))
					Func[i](sprite);
			}

			drawBitIcon(Rects[i].x, Rects[i].y+1, Icons + i*8, down ? (tic_color_white) : (tic_color_black));

			if(!down) drawBitIcon(Rects[i].x, Rects[i].y, Icons + i*8, (tic_color_white));
		}
	}
}

static void drawRGBSlider(Sprite* sprite, s32 x, s32 y, u8* value)
{
	enum {Size = CANVAS_SIZE, Max = 255};

	{
		static const u8 Icon[] =
		{
			0b11100000,
			0b11100000,
			0b11100000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
		};

		tic_rect rect = {x, y-2, Size, 5};

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			if(checkMouseDown(&rect, tic_mouse_left))
			{
				s32 mx = getMouseX() - x;
				*value = mx * Max / (Size-1);
			}
		}

		sprite->tic->api.rect(sprite->tic, x, y+1, Size, 1, (tic_color_black));
		sprite->tic->api.rect(sprite->tic, x, y, Size, 1, (tic_color_white));

		{
			s32 offset = x + *value * (Size-1) / Max - 1;
			drawBitIcon(offset, y, Icon, (tic_color_black));
			drawBitIcon(offset, y-1, Icon, (tic_color_white));
		}

		{
			char buf[] = "FF";
			sprintf(buf, "%02X", *value);
			sprite->tic->api.text(sprite->tic, buf, x - 18, y - 2, (tic_color_dark_gray), false);
		}
	}

	{
		static const u8 Icon[] =
		{
			0b01000000,
			0b11000000,
			0b01000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
		};

		tic_rect rect = {x - 4, y - 1, 2, 3};

		bool down = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			if(checkMouseDown(&rect, tic_mouse_left))
				down = true;

			if(checkMouseClick(&rect, tic_mouse_left))
				(*value)--;
		}

		if(down)
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_white));
		}
		else
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_black));
			drawBitIcon(rect.x, rect.y, Icon, (tic_color_white));
		}
	}

	{
		static const u8 Icon[] =
		{
			0b10000000,
			0b11000000,
			0b10000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
			0b00000000,
		};

		tic_rect rect = {x + Size + 2, y - 1, 2, 3};

		bool down = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			if(checkMouseDown(&rect, tic_mouse_left))
				down = true;

			if(checkMouseClick(&rect, tic_mouse_left))
				(*value)++;
		}

		if(down)
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_white));
		}
		else
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_black));
			drawBitIcon(rect.x, rect.y, Icon, (tic_color_white));
		}
	}
}

static void pasteColor(Sprite* sprite)
{
	fromClipboard(getBankPalette()->data, sizeof(tic_palette), false, true);
	fromClipboard(&getBankPalette()->colors[sprite->color], sizeof(tic_rgb), false, true);
}

static void drawRGBTools(Sprite* sprite, s32 x, s32 y)
{
	{
		enum{Size = 5};
		static const u8 Icon[] = 
		{
			0b11110000,
			0b10010000,
			0b10111000,
			0b11101000,
			0b00111000,
			0b00000000,
			0b00000000,
			0b00000000,	
		};

		tic_rect rect = {x, y, Size, Size};

		bool over = false;
				bool down = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			showTooltip("COPY PALETTE");
			over = true;

			if(checkMouseDown(&rect, tic_mouse_left))
				down = true;

			if(checkMouseClick(&rect, tic_mouse_left))
				toClipboard(getBankPalette()->data, sizeof(tic_palette), false);
		}

		if(down)
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_light_blue));
		}
		else
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_black));
			drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_light_blue : tic_color_white));
		}
	}

	{
		enum{Size = 5};
		static const u8 Icon[] = 
		{
			0b01110000,
			0b10001000,
			0b11111000,
			0b11011000,
			0b11111000,
			0b00000000,
			0b00000000,
			0b00000000,
		};

		tic_rect rect = {x + 8, y, Size, Size};
		bool over = false;
		bool down = false;

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			showTooltip("PASTE PALETTE");
			over = true;

			if(checkMouseDown(&rect, tic_mouse_left))
				down = true;

			if(checkMouseClick(&rect, tic_mouse_left))
			{
				pasteColor(sprite);
			}
		}

		if(down)
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_light_blue));
		}
		else
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_black));
			drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_light_blue : tic_color_white));
		}
	}
}

static void drawRGBSliders(Sprite* sprite, s32 x, s32 y)
{
	enum{Gap = 6, Count = sizeof(tic_rgb)};

	u8* data = &getBankPalette()->data[sprite->color * Count];

	for(s32 i = 0; i < Count; i++)
		drawRGBSlider(sprite, x, y + Gap*i, &data[i]);

	drawRGBTools(sprite, x - 18, y + 26);
}

static void drawRGBSlidersOvr(Sprite* sprite, s32 x, s32 y)
{
	enum{Gap = 6, Count = sizeof(tic_rgb), Size = CANVAS_SIZE, Max = 255};

	u8* data = &getBankPalette()->data[sprite->color * Count];

	for(s32 i = 0; i < Count; i++)
	{
		sprite->tic->api.pixel(sprite->tic, x + data[i] * (Size-1) / Max, y + Gap*i, sprite->color);
	}
}

static void drawPalette(Sprite* sprite, s32 x, s32 y)
{
	tic_rect rect = {x, y, PALETTE_WIDTH-1, PALETTE_HEIGHT-1};

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		bool left = checkMouseDown(&rect, tic_mouse_left);
		bool right = checkMouseDown(&rect, tic_mouse_right);

		if(left || right)
		{
			s32 mx = getMouseX() - x;
			s32 my = getMouseY() - y;

			mx /= PALETTE_CELL_SIZE;
			my /= PALETTE_CELL_SIZE;

			s32 index = mx + my * PALETTE_COLS;

			if(left) sprite->color = index;
			if(right) sprite->color2 = index;
		}
	}

	sprite->tic->api.rect(sprite->tic, rect.x-1, rect.y-1, rect.w+2, rect.h+2, (tic_color_white));
	sprite->tic->api.rect(sprite->tic, rect.x-1, rect.y+rect.h+1, PALETTE_WIDTH+1, 1, (tic_color_black));

	{
		s32 offsetX = x + (sprite->color % PALETTE_COLS) * PALETTE_CELL_SIZE;
		s32 offsetY = y + (sprite->color / PALETTE_COLS) * PALETTE_CELL_SIZE;

		if(offsetY > y)
			sprite->tic->api.rect(sprite->tic, offsetX - 2, rect.y + rect.h+2, PALETTE_CELL_SIZE+3, 1, (tic_color_black));		
	}

	{
		static const u8 Icon[] = 
		{
			0b01000000,
			0b11111111,
			0b00000000,
			0b00000010,
			0b11111111,
			0b00000000,
			0b00010000,
			0b11111111,
		};

		tic_rect rect = {x + PALETTE_WIDTH + 3, y + (PALETTE_HEIGHT-8)/2-1, 8, 8};

		bool down = false;
		bool over = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			over = true;

			showTooltip("EDIT PALETTE");

			if(checkMouseDown(&rect, tic_mouse_left))
				down = true;

			if(checkMouseClick(&rect, tic_mouse_left))
				sprite->editPalette = !sprite->editPalette;
		}

		if(sprite->editPalette || down)
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (over ? tic_color_light_blue : tic_color_white));
		}
		else
		{
			drawBitIcon(rect.x, rect.y+1, Icon, (tic_color_black));
			drawBitIcon(rect.x, rect.y, Icon, (over ? tic_color_light_blue : tic_color_white));			
		}
	}
}

static void drawPaletteOvr(Sprite* sprite, s32 x, s32 y)
{
	for(s32 row = 0, i = 0; row < PALETTE_ROWS; row++)
		for(s32 col = 0; col < PALETTE_COLS; col++)
			sprite->tic->api.rect(sprite->tic, x + col * PALETTE_CELL_SIZE, y + row * PALETTE_CELL_SIZE, PALETTE_CELL_SIZE-1, PALETTE_CELL_SIZE-1, i++);

	{
		s32 offsetX = x + (sprite->color % PALETTE_COLS) * PALETTE_CELL_SIZE;
		s32 offsetY = y + (sprite->color / PALETTE_COLS) * PALETTE_CELL_SIZE;

		sprite->tic->api.rect(sprite->tic, offsetX - 1, offsetY - 1, PALETTE_CELL_SIZE + 1, PALETTE_CELL_SIZE + 1, sprite->color);
		sprite->tic->api.rect_border(sprite->tic, offsetX - 2, offsetY - 2, PALETTE_CELL_SIZE + 3, PALETTE_CELL_SIZE + 3, (tic_color_white));
	}

	{
		static const u8 Icon[] =
		{
			0b00000000,
			0b00111000,
			0b01000100,
			0b01000100,
			0b01000100,
			0b00111000,
			0b00000000,
			0b00000000,
		};

		s32 offsetX = x + (sprite->color2 % PALETTE_COLS) * PALETTE_CELL_SIZE;
		s32 offsetY = y + (sprite->color2 / PALETTE_COLS) * PALETTE_CELL_SIZE;

		drawBitIcon(offsetX, offsetY, Icon, sprite->color2 == (tic_color_white) ? (tic_color_black) : (tic_color_white));
	}
}

static void selectSprite(Sprite* sprite, s32 x, s32 y)
{
	{
		s32 size = TIC_SPRITESHEET_SIZE - sprite->size;		
		if(x < 0) x = 0;
		if(y < 0) y = 0;
		if(x > size) x = size;
		if(y > size) y = size;
	}

	x /= TIC_SPRITESIZE;
	y /= TIC_SPRITESIZE;

	sprite->index -= sprite->index % TIC_BANK_SPRITES;
	sprite->index += x + y * SHEET_COLS;

	clearCanvasSelection(sprite);
}

static void updateSpriteSize(Sprite* sprite, s32 size)
{
	if(size != sprite->size)
	{
		sprite->size = size;
		selectSprite(sprite, getIndexPosX(sprite), getIndexPosY(sprite));
	}
}

static void drawSheet(Sprite* sprite, s32 x, s32 y)
{
	tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

	sprite->tic->api.rect_border(sprite->tic, rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2, (tic_color_white));
	sprite->tic->api.rect(sprite->tic, rect.x, rect.y, rect.w, rect.h, (tic_color_black));

	if(checkMousePos(&rect))
	{
		setCursor(tic_cursor_hand);

		if(checkMouseDown(&rect, tic_mouse_left))
		{
			s32 offset = (sprite->size - TIC_SPRITESIZE) / 2;
			selectSprite(sprite, getMouseX() - x - offset, getMouseY() - y - offset);
		}
	}
}

static void drawSheetOvr(Sprite* sprite, s32 x, s32 y)
{
	tic_rect rect = {x, y, TIC_SPRITESHEET_SIZE, TIC_SPRITESHEET_SIZE};

	for(s32 j = 0, index = (sprite->index - sprite->index % TIC_BANK_SPRITES); j < rect.h; j += TIC_SPRITESIZE)
		for(s32 i = 0; i < rect.w; i += TIC_SPRITESIZE, index++)
			sprite->tic->api.sprite(sprite->tic, sprite->src, index, x + i, y + j, NULL, 0);
	{
		s32 bx = getIndexPosX(sprite) + x - 1;
		s32 by = getIndexPosY(sprite) + y - 1;

		sprite->tic->api.rect_border(sprite->tic, bx, by, sprite->size + 2, sprite->size + 2, (tic_color_white));
	}
}

static void flipSpriteHorz(Sprite* sprite)
{
	tic_rect rect = getSpriteRect(sprite);
	s32 r = rect.x + rect.w/2;
	s32 b = rect.y + rect.h;

	for(s32 y = rect.y; y < b; y++)
		for(s32 x = rect.x, i = rect.x + rect.w - 1; x < r; x++, i--)
		{
			u8 color = getSheetPixel(sprite, x, y);
			setSheetPixel(sprite, x, y, getSheetPixel(sprite, i, y));
			setSheetPixel(sprite, i, y, color);
		}

	history_add(sprite->history);
}

static void flipSpriteVert(Sprite* sprite)
{
	tic_rect rect = getSpriteRect(sprite);
	s32 r = rect.x + rect.w;
	s32 b = rect.y + rect.h/2;

	for(s32 y = rect.y, i = rect.y + rect.h - 1; y < b; y++, i--)
		for(s32 x = rect.x; x < r; x++)
		{
			u8 color = getSheetPixel(sprite, x, y);
			setSheetPixel(sprite, x, y, getSheetPixel(sprite, x, i));
			setSheetPixel(sprite, x, i, color);
		}

	history_add(sprite->history);
}

static void rotateSprite(Sprite* sprite)
{
	const s32 Size = sprite->size;
	u8* buffer = (u8*)malloc(Size * Size);

	if(buffer)
	{
		{
			tic_rect rect = getSpriteRect(sprite);
			s32 r = rect.x + rect.w;
			s32 b = rect.y + rect.h;

			for(s32 y = rect.y, i = 0; y < b; y++)
				for(s32 x = rect.x; x < r; x++)
					buffer[i++] = getSheetPixel(sprite, x, y);

			for(s32 y = rect.y, j = 0; y < b; y++, j++)
				for(s32 x = rect.x, i = 0; x < r; x++, i++)
					setSheetPixel(sprite, x, y, buffer[j + (Size-i-1)*Size]);

			history_add(sprite->history);
		}

		free(buffer);
	}
}

static void deleteSprite(Sprite* sprite)
{
	tic_rect rect = getSpriteRect(sprite);
	s32 r = rect.x + rect.w;
	s32 b = rect.y + rect.h;

	for(s32 y = rect.y; y < b; y++)
		for(s32 x = rect.x; x < r; x++)
			setSheetPixel(sprite, x, y, sprite->color2);

	clearCanvasSelection(sprite);

	history_add(sprite->history);
}

static void(* const SpriteToolsFunc[])(Sprite*) = {flipSpriteHorz, flipSpriteVert, rotateSprite, deleteSprite};
static void(* const CanvasToolsFunc[])(Sprite*) = {flipCanvasHorz, flipCanvasVert, rotateCanvas, deleteCanvas};

static void drawSpriteTools(Sprite* sprite, s32 x, s32 y)
{
	static const u8 Icons[] =
	{
		0b11101110,
		0b11010110,
		0b11101110,
		0b11101110,
		0b11101110,
		0b11010110,
		0b11101110,
		0b00000000,

		0b11111110,
		0b11111110,
		0b10111010,
		0b01000100,
		0b10111010,
		0b11111110,
		0b11111110,
		0b00000000,

		0b00111000,
		0b01000100,
		0b10010101,
		0b10001110,
		0b10000100,
		0b01000000,
		0b00111000,
		0b00000000,

		0b00111110,
		0b01111111,
		0b00101010,
		0b00101010,
		0b00101010,
		0b00101010,
		0b00111110,
		0b00000000,
	};
	static const char* Tooltips[] = {"FLIP HORZ [5]", "FLIP VERT [6]", "ROTATE [7]", "ERASE [8]"};

	enum{Gap = TIC_SPRITESIZE + 3};

	for(s32 i = 0; i < COUNT_OF(Icons)/BITS_IN_BYTE; i++)
	{
		bool pushed = false;
		bool over = false;
		
		tic_rect rect = {x + i * Gap, y, TIC_SPRITESIZE, TIC_SPRITESIZE};

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			over = true;

			showTooltip(Tooltips[i]);

			if(checkMouseDown(&rect, tic_mouse_left)) pushed = true;

			if(checkMouseClick(&rect, tic_mouse_left))
			{		
				if(hasCanvasSelection(sprite))
				{
					CanvasToolsFunc[i](sprite);
				}
				else
				{
					SpriteToolsFunc[i](sprite);
					clearCanvasSelection(sprite);
				}
			}
		}

		if(pushed)
		{
			drawBitIcon(rect.x, y + 1, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_blue : tic_color_white));
		}
		else
		{
			drawBitIcon(rect.x, y+1, Icons + i*BITS_IN_BYTE, (tic_color_black));
			drawBitIcon(rect.x, y, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_blue : tic_color_white));
		}
	}
}

static void drawTools(Sprite* sprite, s32 x, s32 y)
{
	static const u8 Icons[] = 
	{
		0b00001000,
		0b00011100,
		0b00111110,
		0b01111100,
		0b10111000,
		0b10010000,
		0b11100000,
		0b00000000,

		0b00111000,
		0b00111000,
		0b01111100,
		0b00101000,
		0b00101000,
		0b00101000,
		0b00010000,
		0b00000000,

		0b10101010,
		0b00000000,
		0b10000010,
		0b00000000,
		0b10000010,
		0b00000000,
		0b10101010,
		0b00000000,

		0b00001000,
		0b00000100,
		0b00000010,
		0b01111111,
		0b10111110,
		0b10011100,
		0b10001000,
		0b00000000,
	};

	enum{Gap = TIC_SPRITESIZE + 3};

	for(s32 i = 0; i < COUNT_OF(Icons)/BITS_IN_BYTE; i++)
	{
		tic_rect rect = {x + i * Gap, y, TIC_SPRITESIZE, TIC_SPRITESIZE};

		bool over = false;
		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);
			over = true;

			static const char* Tooltips[] = {"BRUSH [1]", "COLOR PICKER [2]", "SELECT [3]", "FILL [4]"};

			showTooltip(Tooltips[i]);

			if(checkMouseClick(&rect, tic_mouse_left))
			{				
				sprite->mode = i;

				clearCanvasSelection(sprite);
			}
		}

		bool pushed = i == sprite->mode;

		if(pushed)
		{
			static const u8 Icon[] = 
			{
				0b01111100,
				0b00111000,
				0b00010000,
				0b00000000,
				0b00000000,
				0b00000000,
				0b00000000,
				0b00000000,
			};

			drawBitIcon(rect.x, y - 4, Icon, (tic_color_black));
			drawBitIcon(rect.x, y - 5, Icon, (tic_color_white));

			drawBitIcon(rect.x, y + 1, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_blue : tic_color_white));
		}
		else
		{
			drawBitIcon(rect.x, y+1, Icons + i*BITS_IN_BYTE, (tic_color_black));
			drawBitIcon(rect.x, y, Icons + i*BITS_IN_BYTE, (over ? tic_color_light_blue : tic_color_white));
		}
	}

	drawSpriteTools(sprite, x + COUNT_OF(Icons)/BITS_IN_BYTE * Gap + 1, y);
}

static void copyToClipboard(Sprite* sprite)
{
	s32 size = sprite->size * sprite->size * TIC_PALETTE_BPP / BITS_IN_BYTE;
	u8* buffer = malloc(size);

	if(buffer)
	{
		tic_rect rect = getSpriteRect(sprite);
		s32 r = rect.x + rect.w;
		s32 b = rect.y + rect.h;

		for(s32 y = rect.y, i = 0; y < b; y++)
			for(s32 x = rect.x; x < r; x++)
				tic_tool_poke4(buffer, i++, getSheetPixel(sprite, x, y) & 0xf);

		toClipboard(buffer, size, true);

		free(buffer);
	}
}

static void cutToClipboard(Sprite* sprite)
{
	copyToClipboard(sprite);
	deleteSprite(sprite);
}

static void copyFromClipboard(Sprite* sprite)
{
	if(sprite->editPalette)
		pasteColor(sprite);

	s32 size = sprite->size * sprite->size * TIC_PALETTE_BPP / BITS_IN_BYTE;
	u8* buffer = malloc(size);

	if(buffer)
	{
		if(fromClipboard(buffer, size, true, false))
		{
			tic_rect rect = getSpriteRect(sprite);
			s32 r = rect.x + rect.w;
			s32 b = rect.y + rect.h;

			for(s32 y = rect.y, i = 0; y < b; y++)
				for(s32 x = rect.x; x < r; x++)
					setSheetPixel(sprite, x, y, tic_tool_peek4(buffer, i++));

			history_add(sprite->history);
		}

		free(buffer);

	}
}

static void upSprite(Sprite* sprite)
{
	if(getIndexPosY(sprite) > 0) sprite->index -= SHEET_COLS;
}

static void downSprite(Sprite* sprite)
{
	if(getIndexPosY(sprite) < TIC_SPRITESHEET_SIZE - sprite->size) sprite->index += SHEET_COLS;
}

static void leftSprite(Sprite* sprite)
{
	if(getIndexPosX(sprite) > 0) sprite->index--;
}

static void rightSprite(Sprite* sprite)
{
	if(getIndexPosX(sprite) < TIC_SPRITESHEET_SIZE - sprite->size) sprite->index++;
}

static void undo(Sprite* sprite)
{
	history_undo(sprite->history);
}

static void redo(Sprite* sprite)
{
	history_redo(sprite->history);
}

static void switchBanks(Sprite* sprite)
{
	bool bg = sprite->index < TIC_BANK_SPRITES;

	sprite->index += bg ? TIC_BANK_SPRITES : -TIC_BANK_SPRITES;
	
	clearCanvasSelection(sprite);
}

static void processKeyboard(Sprite* sprite)
{
	tic_mem* tic = sprite->tic;

	if(tic->ram.input.keyboard.data == 0) return;

	switch(getClipboardEvent())
	{
	case TIC_CLIPBOARD_CUT: cutToClipboard(sprite); break;
	case TIC_CLIPBOARD_COPY: copyToClipboard(sprite); break;
	case TIC_CLIPBOARD_PASTE: copyFromClipboard(sprite); break;
	default: break;
	}

	bool ctrl = tic->api.key(tic, tic_key_ctrl);

	if(ctrl)
	{	
		if(keyWasPressed(tic_key_z)) 		undo(sprite);
		else if(keyWasPressed(tic_key_y))	redo(sprite);
	}
	else
	{
		if(hasCanvasSelection(sprite))
		{
			if(keyWasPressed(tic_key_up)) 			upCanvas(sprite);
			else if(keyWasPressed(tic_key_down)) 	downCanvas(sprite);
			else if(keyWasPressed(tic_key_left)) 	leftCanvas(sprite);
			else if(keyWasPressed(tic_key_right)) 	rightCanvas(sprite);
			else if(keyWasPressed(tic_key_delete)) 	deleteCanvas(sprite);
		}
		else
		{
			if(keyWasPressed(tic_key_up)) 			upSprite(sprite);
			else if(keyWasPressed(tic_key_down)) 	downSprite(sprite);
			else if(keyWasPressed(tic_key_left)) 	leftSprite(sprite);
			else if(keyWasPressed(tic_key_right)) 	rightSprite(sprite);
			else if(keyWasPressed(tic_key_delete)) 	deleteSprite(sprite);
			else if(keyWasPressed(tic_key_tab)) 		switchBanks(sprite);

			if(!sprite->editPalette)
			{

				if(keyWasPressed(tic_key_1)) 		sprite->mode = SPRITE_DRAW_MODE;
				else if(keyWasPressed(tic_key_2)) 	sprite->mode = SPRITE_PICK_MODE;
				else if(keyWasPressed(tic_key_3)) 	sprite->mode = SPRITE_SELECT_MODE;
				else if(keyWasPressed(tic_key_4)) 	sprite->mode = SPRITE_FILL_MODE;

				else if(keyWasPressed(tic_key_5)) 	flipSpriteHorz(sprite);
				else if(keyWasPressed(tic_key_6)) 	flipSpriteVert(sprite);
				else if(keyWasPressed(tic_key_7)) 	rotateSprite(sprite);
				else if(keyWasPressed(tic_key_8)) 	deleteSprite(sprite);

				if(sprite->mode == SPRITE_DRAW_MODE)
				{
					if(keyWasPressed(tic_key_leftbracket)) {if(sprite->brushSize > 1) sprite->brushSize--;}
					else if(keyWasPressed(tic_key_rightbracket)) {if(sprite->brushSize < 4) sprite->brushSize++;}
				}				
			}
		}
	}
}

static void drawSpriteToolbar(Sprite* sprite)
{
	sprite->tic->api.rect(sprite->tic, 0, 0, TIC80_WIDTH, TOOLBAR_SIZE, (tic_color_white));

	// draw sprite size control
	{
		tic_rect rect = {TIC80_WIDTH - 58, 1, 23, 5};

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			showTooltip("CANVAS ZOOM");

			if(checkMouseDown(&rect, tic_mouse_left))
			{
				s32 mx = getMouseX() - rect.x;
				mx /= 6;

				s32 size = 1;
				while(mx--) size <<= 1;

				updateSpriteSize(sprite, size * TIC_SPRITESIZE);
			}
		}

		for(s32 i = 0; i < 4; i++)
			sprite->tic->api.rect(sprite->tic, rect.x + i*6, 1, 5, 5, (tic_color_black));

		sprite->tic->api.rect(sprite->tic, rect.x, 2, 23, 3, (tic_color_black));
		sprite->tic->api.rect(sprite->tic, rect.x+1, 3, 21, 1, (tic_color_white));

		s32 size = sprite->size / TIC_SPRITESIZE, val = 0;
		while(size >>= 1) val++;

		sprite->tic->api.rect(sprite->tic, rect.x + val*6, 1, 5, 5, (tic_color_black));
		sprite->tic->api.rect(sprite->tic, rect.x+1 + val*6, 2, 3, 3, (tic_color_white));
	}

	bool bg = sprite->index < TIC_BANK_SPRITES;

	{
		static const char Label[] = "BG";
		tic_rect rect = {TIC80_WIDTH - 2 * TIC_FONT_WIDTH - 2, 0, 2 * TIC_FONT_WIDTH + 1, TIC_SPRITESIZE-1};
		sprite->tic->api.rect(sprite->tic, rect.x, rect.y, rect.w, rect.h, bg ? (tic_color_black) : (tic_color_gray));
		sprite->tic->api.fixed_text(sprite->tic, Label, rect.x+1, rect.y+1, (tic_color_white), false);

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			showTooltip("TILES [tab]");

			if(!bg && checkMouseClick(&rect, tic_mouse_left))
			{
				sprite->index -= TIC_BANK_SPRITES;
				clearCanvasSelection(sprite);
			}
		}
	}

	{
		static const char Label[] = "FG";
		tic_rect rect = {TIC80_WIDTH - 4 * TIC_FONT_WIDTH - 4, 0, 2 * TIC_FONT_WIDTH + 1, TIC_SPRITESIZE-1};
		sprite->tic->api.rect(sprite->tic, rect.x, rect.y, rect.w, rect.h, bg ? (tic_color_gray) : (tic_color_black));
		sprite->tic->api.fixed_text(sprite->tic, Label, rect.x+1, rect.y+1, (tic_color_white), false);

		if(checkMousePos(&rect))
		{
			setCursor(tic_cursor_hand);

			showTooltip("SPRITES [tab]");

			if(bg && checkMouseClick(&rect, tic_mouse_left))
			{
				sprite->index += TIC_BANK_SPRITES;
				clearCanvasSelection(sprite);
			}
		}
	}
}

static void tick(Sprite* sprite)
{
	// process scroll
	{
		tic80_input* input = &sprite->tic->ram.input;

		if(input->mouse.scrolly)
		{
			s32 size = sprite->size;
			s32 delta = input->mouse.scrolly;

			if(delta > 0) 
			{
				if(size < (TIC_SPRITESIZE * TIC_SPRITESIZE)) size <<= 1;					
			}
			else if(size > TIC_SPRITESIZE) size >>= 1;

			updateSpriteSize(sprite, size);	
		}
	}

	processKeyboard(sprite);

	sprite->tic->api.clear(sprite->tic, (tic_color_gray));

	drawCanvas(sprite, 24, 20);
	drawMoveButtons(sprite);

	sprite->editPalette 
		? drawRGBSliders(sprite, 24, 91) 
		: drawTools(sprite, 12, 96);

	drawPalette(sprite, 24, 112);
	drawSheet(sprite, TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, 7);
	
	drawSpriteToolbar(sprite);
	drawToolbar(sprite->tic, (tic_color_gray), false);

	sprite->tickCounter++;
}

static void onStudioEvent(Sprite* sprite, StudioEvent event)
{
	switch(event)
	{
	case TIC_TOOLBAR_CUT: cutToClipboard(sprite); break;
	case TIC_TOOLBAR_COPY: copyToClipboard(sprite); break;
	case TIC_TOOLBAR_PASTE: copyFromClipboard(sprite); break;
	case TIC_TOOLBAR_UNDO: undo(sprite); break;
	case TIC_TOOLBAR_REDO: redo(sprite); break;
	}
}

static void scanline(tic_mem* tic, s32 row, void* data)
{
	if(row == 0)
		memcpy(tic->ram.vram.palette.data, getConfig()->cart->bank0.palette.data, sizeof(tic_palette));
}

static void overline(tic_mem* tic, void* data)
{
	Sprite* sprite = (Sprite*)data;

	if(sprite->editPalette)
		drawRGBSlidersOvr(sprite, 24, 91);

	drawCanvasOvr(sprite, 24, 20);
	drawPaletteOvr(sprite, 24, 112);
	drawSheetOvr(sprite, TIC80_WIDTH - TIC_SPRITESHEET_SIZE - 1, 7);
}

void initSprite(Sprite* sprite, tic_mem* tic, tic_tiles* src)
{
	if(sprite->select.back == NULL) sprite->select.back = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);
	if(sprite->select.front == NULL) sprite->select.front = (u8*)malloc(CANVAS_SIZE*CANVAS_SIZE);
	if(sprite->history) history_delete(sprite->history);
	
	*sprite = (Sprite)
	{
		.tic = tic,
		.tick = tick,
		.tickCounter = 0,
		.src = src,
		.index = 0,
		.color = 1,
		.color2 = 0,
		.size = TIC_SPRITESIZE,
		.editPalette = false,
		.brushSize = 1,
		.select = 
		{
			.rect = {0,0,0,0},
			.start = {0,0},
			.drag = false,
			.back = sprite->select.back,
			.front = sprite->select.front,
		},
		.mode = SPRITE_DRAW_MODE,
		.history = history_create(src, TIC_SPRITES * sizeof(tic_tile)),
		.event = onStudioEvent,
		.overline = overline,
		.scanline = scanline,
	};
}