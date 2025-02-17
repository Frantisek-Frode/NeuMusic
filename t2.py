from pynput import keyboard

print(dir(keyboard.Key))

def on_press(key):
    print(str(key), key)
    try:
        if key == keyboard.Key.media_play_pause:
            print("Play/Pause key pressed")
        # elif key == keyboard.Key.media_stop:
            # print("Stop key pressed")
        elif key == keyboard.Key.media_previous:
            print("Previous track key pressed")
        elif key == keyboard.Key.media_next:
            print("Next track key pressed")
        elif key == keyboard.Key.media_volume_up:
            print("Volume Up key pressed")
        elif key == keyboard.Key.media_volume_down:
            print("Volume Down key pressed")
    except AttributeError as e:
        print(e)


with keyboard.Listener(on_press=on_press, on_release=None) as listener_thread:
    listener_thread.join()

