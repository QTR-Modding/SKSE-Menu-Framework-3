#pragma once


class Theme {
public:
    static inline float WindowBlurStrength = 0.0f;
    static std::vector<std::string> GetJsonFiles();
    static void LoadJsonStyle(const std::string& path);
};
