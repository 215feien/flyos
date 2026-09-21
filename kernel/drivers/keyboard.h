#ifndef MYOS_KEYBOARD_H
#define MYOS_KEYBOARD_H

void keyboard_init(void);
int  keyboard_getchar_nonblock(void);   /* 无数据返回 -1 */

#endif
