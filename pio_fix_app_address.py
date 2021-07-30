Import("env")

def before_upload(source, target, env):
    print("************* pio_fix_app_address.py(before_upload)")
    print("*** Fixing incorrect app address for OTA based deployments in UPLOADCMD env variable.")
    print("*** See: https://github.com/platformio/platform-espressif32/issues/403")
    print("*** hard coded address 0x10000 in ~/.platformio/platforms/espressif32/builder/main.py")
    print("Current value: ", env.Dump("UPLOADCMD"))
    env.Replace(UPLOADCMD = '"$PYTHONEXE" "$UPLOADER" $UPLOADERFLAGS 0x20000 $SOURCE')
    print("Updated value: ", env.Dump("UPLOADCMD"))
    print("************** done.")

env.AddPreAction("upload", before_upload)