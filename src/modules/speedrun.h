#pragma once
#include <string>

// Fitur gaya speedrun: unlock shop, tombol restart (Paused.RestartP), kontrol RNG,
// Extra Traps, Lava Mode, dan alat dump class game.
namespace speedrun {
    extern bool autoUnlockShop;   // buka semua item shop otomatis
    extern bool restartButton;    // tombol restart mengambang di dalam gameplay
    extern float restartPosX;     // posisi tombol restart (-1 = default)
    extern float restartPosY;

    // Kontrol RNG (0 = acak / bawaan game)
    extern int  ratChoice;        // 1 = RemoteRatHang1 (Kiri), 2 = RemoteRatHang2 (Kanan)
    extern int  momRoute;         // 1 = Run1 (Tunnel), 2 = Run2 (Elevator)
    extern int  vaseChoice;       // 1..N = posisi guci Grandpa
    extern int  vaseCount;        // jumlah posisi guci yang diketahui (0 = belum tahu)

    // Ekstra
    extern bool extraTraps;
    extern bool lavaMode;

    void Init();
    void Update();
    void ClearCache();
    void DrawRestartButton();
    void DrawMenu(float colW, float colH, float colGap);

    bool IsGameplay();                 // true jika scene aktif adalah "Scene" (sedang bermain)
    bool IsPaused();                   // true jika Time.timeScale == 0
    void RestartRun(bool allowWhilePaused);   // sama seperti Paused.RestartP()

    int  UnlockAllShop();     // jumlah item baru dibuka (-1 = gagal)
    bool DumpClasses();       // tulis dump_classes.txt ke folder data game
}
