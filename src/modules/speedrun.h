#pragma once
#include <string>

// Fitur gaya speedrun: unlock shop, tombol restart, dan alat dump class game.
namespace speedrun {
    extern bool autoUnlockShop;   // buka semua item shop otomatis saat game/scene dimuat
    extern bool restartButton;    // tombol restart mengambang di dalam gameplay
    extern float restartPosX;     // posisi tombol restart (-1 = default)
    extern float restartPosY;

    void Init();
    void Update();
    void ClearCache();
    void DrawRestartButton();                 // dipanggil tiap frame (overlay)
    void DrawMenu(float colW, float colH, float colGap);  // isi tab Speedrun

    int  UnlockAllShop();                     // mengembalikan jumlah item yang baru dibuka (-1 = gagal)
    bool DumpClasses();                       // tulis dump_classes.txt ke folder data game
}
