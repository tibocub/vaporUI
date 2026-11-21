// Decoder regression test: feeds raw terminal bytes, checks the events.
// Run with `make test`.
#include <assert.h>
#include <string.h>
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
static int probe_value=-1;static int probe(void){return probe_value;} // VuiModifiers held, -1 unknown
static VuiEvent run(const char *s){VuiInputDecoder d;vui_input_reset(&d);VuiEvent e={0};for(;*s;s++){VuiEvent x=vui_input_feed(&d,(uint8_t)*s);if(x.type)e=x;}return e;}
static const char *hook_name;
static const char *hook_fr(void){ return hook_name ? hook_name : "fr"; }
int main(void){
 VuiEvent e=run("\x1b" "x"); assert(e.key.key==VUI_KEY_CHAR&&e.key.character=='x'&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b[1;5C"); assert(e.key.key==VUI_KEY_RIGHT&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[36;5u"); assert(e.key.character=='4'&&e.key.modifiers==(VUI_MOD_CTRL|VUI_MOD_SHIFT)&&e.key.base=='4'); // US: $ is Shift+4
 e=run("\x1bQ"); assert(e.key.character=='q'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 e=run("\x1b[81;4u"); assert(e.key.character=='q'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 e=run("\x1b[113;4u"); assert(e.key.character=='q'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
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
 e=run("\x1b[27;6;36~"); assert(e.key.character=='4'&&e.key.modifiers==(VUI_MOD_CTRL|VUI_MOD_SHIFT));
 e=run("\x1b[27;5;13~"); assert(e.key.key==VUI_KEY_ENTER&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[233;5u"); assert(e.key.character==0xe9&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\xc3\xa9"); assert(e.type==VUI_EVENT_TEXT&&e.text.codepoint==0xe9);
 e=run("\xe2\x82\xac"); assert(e.type==VUI_EVENT_TEXT&&e.text.codepoint==0x20ac);
 e=run("\x1b[1;2H"); assert(e.key.key==VUI_KEY_HOME&&e.key.modifiers==VUI_MOD_SHIFT);
 e=run("\x1b[1;2F"); assert(e.key.key==VUI_KEY_END&&e.key.modifiers==VUI_MOD_SHIFT);
 e=run("\x1b[5;2~"); assert(e.key.key==VUI_KEY_PAGE_UP&&e.key.modifiers==VUI_MOD_SHIFT);
 e=run("\x1b[6;2~"); assert(e.key.key==VUI_KEY_PAGE_DOWN&&e.key.modifiers==VUI_MOD_SHIFT);
 e=run("\x1b[1;6F"); assert(e.key.modifiers==(VUI_MOD_SHIFT|VUI_MOD_CTRL));
 // Shift on any letter, accents included, is lowercase + Shift
 e=run("\x1b\xc3\x89"); assert(e.key.character==0xe9&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 e=run("\x1b[201;4u"); assert(e.key.character==0xe9&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 // base key: US layout by default
 e=run("\x1bq"); assert(e.key.base=='q'&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b!"); assert(e.key.base=='1'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 // kitty alternate keys (code:shifted:base) win over any table
 e=run("\x1b[38::49;3u"); assert(e.key.character=='&'&&e.key.base=='1'&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b[49:33:49;4u"); assert(e.key.character=='1'&&e.key.base=='1'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 // French layout table
 assert(vui_set_layout("fr")==0&&vui_set_layout("klingon")==-1);
 e=run("\x1b&"); assert(e.key.base=='1'&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b" "1"); assert(e.key.base=='1'&&e.key.character=='&'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 e=run("\x1b\xc3\xa9"); assert(e.key.base=='2'&&e.key.character==0xe9&&e.key.modifiers==VUI_MOD_ALT);
 e=run("\x1b[27;5;233~"); assert(e.key.base=='2'&&e.key.modifiers==VUI_MOD_CTRL);
 e=run("\x1b[233;7u"); assert(e.key.base=='2'&&e.key.modifiers==(VUI_MOD_CTRL|VUI_MOD_ALT));
 e=run("\x1b" "a"); assert(e.key.character=='a'&&e.key.base=='q');
 e=run("\x1b" "A"); assert(e.key.character=='a'&&e.key.base=='q'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 // Linux console: Alt+é and Alt+Shift+é are both ESC 2; the shift probe says which
 vui_input_set_shift_probe(probe);
 probe_value=0; e=run("\x1b" "2"); assert(e.key.base=='2'&&e.key.modifiers==VUI_MOD_ALT);
 probe_value=1; e=run("\x1b" "2"); assert(e.key.base=='2'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 probe_value=0; e=run("\x1b&"); assert(e.key.base=='1'&&e.key.modifiers==VUI_MOD_ALT);
 probe_value=1; e=run("\x1b" "1"); assert(e.key.base=='1'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT));
 probe_value=0; e=run("\x1b" "Q"); assert(e.key.character=='q'&&e.key.modifiers==VUI_MOD_ALT); // Caps Lock, no Shift
 probe_value=-1; e=run("\x1b" "1"); assert(e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT)); // cannot tell: the table's guess
 probe_value=1; e=run("\x1b[233;3u"); assert(e.key.modifiers==VUI_MOD_ALT); // kitty says its own modifiers, probe unused
 probe_value=0; e=run("\x1b" "2"); assert(e.key.character==0xe9&&e.key.base=='2'&&e.key.modifiers==VUI_MOD_ALT); // Alt+é, not Alt+2
 probe_value=1; e=run("\x1b" "2"); assert(e.key.character==0xe9&&e.key.base=='2'&&e.key.modifiers==(VUI_MOD_ALT|VUI_MOD_SHIFT)); // Alt+Shift+é
 probe_value=VUI_MOD_CTRL; e=run("&"); assert(e.type==VUI_EVENT_KEY&&e.key.character=='&'&&e.key.base=='1'&&e.key.modifiers==VUI_MOD_CTRL); // Ctrl+&
 probe_value=VUI_MOD_CTRL|VUI_MOD_SHIFT; e=run("1"); assert(e.key.base=='1'&&e.key.modifiers==(VUI_MOD_CTRL|VUI_MOD_SHIFT)); // Ctrl+Shift+&
 probe_value=VUI_MOD_CTRL|VUI_MOD_SHIFT; e=run("\x01"); assert(e.key.character=='a'&&e.key.modifiers==(VUI_MOD_CTRL|VUI_MOD_SHIFT)); // Ctrl+Shift+a
 probe_value=0; e=run("&"); assert(e.type==VUI_EVENT_TEXT&&e.text.codepoint=='&'); // plain typing stays text
 probe_value=VUI_MOD_SHIFT; e=run("1"); assert(e.type==VUI_EVENT_TEXT); // Shift alone is just a capital / symbol
 vui_input_set_shift_probe(NULL);
 vui_set_layout("us");
 // OS-reported layout names (the backend's hook), used when no name is given and $VUI_LAYOUT is unset
 unsetenv("VUI_LAYOUT");
 vui_layout_hook=hook_fr; assert(vui_set_layout(NULL)==0);
 { const char *names[]={"fr","fr-latin9","fr(oss)","fr+oss","us","de","fr_CH"}; int want[]={1,1,1,1,0,0,0};
   for(int i=0;i<7;i++){ hook_name=names[i]; vui_set_layout("us"); vui_set_layout(NULL); VuiEvent k=run("\033&"); assert((k.key.base=='1')==want[i]); } }
 vui_layout_hook=NULL;
 // other tables: what Alt+<char> reports as its US position
 { struct { const char *lay, *seq; int base, shift; } c[] = {
     { "de", "\033&", '6', 1 }, { "de", "\033/", '7', 1 }, { "de", "\033z", 'y', 0 }, { "de", "\033\xc3\xb6", ';', 0 },
     { "es", "\033&", '6', 1 }, { "es", "\033\xc3\xb1", ';', 0 }, { "it", "\033\xc3\xb2", ';', 0 }, { "it", "\033\"", '2', 1 },
     { "gb", "\033\"", '2', 1 }, { "gb", "\033@", '\'', 1 }, { "gb", "\033&", '7', 1 } };
   for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
     assert(vui_set_layout(c[i].lay) == 0);
     VuiEvent k = run(c[i].seq);
     assert(k.key.base == (unsigned)c[i].base && !!(k.key.modifiers & VUI_MOD_SHIFT) == c[i].shift);
   } }
 vui_set_layout("us");
 puts("input decoder: ok");
 return 0;}
