// Decoder regression test: feeds raw terminal bytes, checks the events.
// Run with `make test`.
#include <assert.h>
#include <string.h>
#include "input.h"
#include <stdio.h>
static VuiEvent run(const char *s){VuiInputDecoder d;vui_input_reset(&d);VuiEvent e={0};for(;*s;s++){VuiEvent x=vui_input_feed(&d,(uint8_t)*s);if(x.type)e=x;}return e;}
int main(void){
 VuiEvent e=run("\x1b" "x"); assert(e.key.key==VUI_KEY_CHAR&&e.key.character=='x'&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b[1;5C"); assert(e.key.key==VUI_KEY_RIGHT&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[36;5u"); assert(e.key.character=='$'&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[97;4u"); assert(e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 e=run("\x1b[Z"); assert(e.key.key==VUI_KEY_TAB&&e.key.modifiers==VUI_MOD_SHIFT);
 e=run("\x1bOA"); assert(e.key.key==VUI_KEY_UP);
 e=run("\x1b\x1b"); assert(e.key.key==VUI_KEY_ESCAPE&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b\x03"); assert(e.key.character=='c'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_CTRL));
 e=run("\x1b" "\xc3\xa9"); assert(e.key.character==0xe9&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1d"); assert(e.key.character==']'&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[24;3~"); assert(e.key.key==VUI_KEY_F12&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1bOS"); assert(e.key.key==VUI_KEY_F4);
 e=run("\x1b[15~"); assert(e.key.key==VUI_KEY_F5);
 e=run("\x1b[<0;10;5M"); assert(e.type==VUI_EVENT_MOUSE&&e.mouse.button==VUI_MOUSE_LEFT&&e.mouse.action==VUI_MOUSE_PRESS&&e.mouse.x==9&&e.mouse.y==4);
 e=run("\x1b[<0;10;5m"); assert(e.mouse.action==VUI_MOUSE_RELEASE);
 e=run("\x1b[<65;1;1M"); assert(e.mouse.button==VUI_MOUSE_WHEEL_DOWN);
 e=run("\x1b[<34;3;4M"); assert(e.mouse.button==VUI_MOUSE_RIGHT&&e.mouse.action==VUI_MOUSE_MOVE);
 e=run("\x1b[<20;3;4M"); assert(e.mouse.modifiers==(VUI_MOD_CTRL|VUI_MOD_SHIFT)&&e.mouse.button==VUI_MOUSE_LEFT);
 { VuiInputDecoder d; const char *in="\x1b[200~a\r\tb\xc3\xa9\x03\x1b[201~z"; int n=0; VuiEvent ev[16]; vui_input_reset(&d);
   for(;*in;in++){VuiEvent x=vui_input_feed(&d,(uint8_t)*in);if(x.type)ev[n++]=x;}
   assert(n==9&&ev[0].type==VUI_EVENT_PASTE_START&&ev[1].text.codepoint=='a'&&ev[2].text.codepoint=='\n'&&ev[3].text.codepoint=='\t'
     &&ev[5].text.codepoint==0xe9&&ev[6].type==VUI_EVENT_TEXT&&ev[6].text.codepoint==3&&ev[7].type==VUI_EVENT_PASTE_END&&ev[8].text.codepoint=='z'); }
 e=run("\x1b[27;5;233~"); assert(e.key.character==0xe9&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[27;6;36~"); assert(e.key.character=='$'&&e.key.modifiers==(VUI_MOD_CTRL|VUI_MOD_SHIFT));
 e=run("\x1b[27;5;13~"); assert(e.key.key==VUI_KEY_ENTER&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[233;5u"); assert(e.key.character==0xe9&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\xc3\xa9"); assert(e.type==VUI_EVENT_TEXT&&e.text.codepoint==0xe9);
 e=run("\xe2\x82\xac"); assert(e.type==VUI_EVENT_TEXT&&e.text.codepoint==0x20ac);
 puts("input decoder: ok");
 return 0;}
