#include <windows.h>

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline,
                     int cmdshow) {
  return MessageBox(NULL, "A message for the Handmade Hero Game",
                    "Handmade Hero", 0);
}
