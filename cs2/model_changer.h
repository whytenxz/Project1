#pragma once

namespace model_changer {
    void Initialize();
    void Scan();
    void OnFrameStage(int stage);
    void OnLevelInit();
    void RequestApply();
    void DrawMenu();

    void PrecacheModelPath(const char* path);
    void SetEntityModel(void* entity, const char* path);
}
