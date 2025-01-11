Import("env")

# Forza PlatformIO a usare LittleFS
print("Executing filesystem_config.py: Forcing LittleFS...")

# Forza PlatformIO a usare LittleFS
env.Replace(
    MKSPIFFSTOOL="mklittlefs",
    FS="LittleFS",
    PIOBUILDFILESYSTEM="littlefs"
)
