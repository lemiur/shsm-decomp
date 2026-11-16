/* top */ int a;
#if 0
int dead1; // gone
#if 1
int dead2;
#else
int dead3;
#endif
#elif 1 /* kept */
int live1;
#endif
#if (0L) // also dead
int dead4;
#else
int live2;
#endif
#if 0
never closed;
