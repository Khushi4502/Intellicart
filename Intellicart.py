import sensor, image, lcd, time
import KPU as kpu
import gc, sys
from fpioa_manager import fm
from machine import UART

# List of labels to send via UART if detected for more than 3 seconds
TARGET_LABELS = ["lotion", "necklace", "chain", "mouse", "envelope",
                 "lipstick","lip rouge", "ballpen", "tennis ball", "Band Aid", "safety pin"]

def main(labels=None, model_addr="/sd/m.kmodel", lcd_rotation=0,
         sensor_hmirror=False, sensor_vflip=False):
    gc.collect()

    # Initialize sensor
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.set_windowing((224, 224))
    sensor.set_hmirror(sensor_hmirror)
    sensor.set_vflip(sensor_vflip)
    sensor.run(1)

    # Initialize LCD
    lcd.init(type=1)
    lcd.rotation(lcd_rotation)
    lcd.clear(lcd.WHITE)

    # Check labels
    if not labels:
        raise Exception("No labels.txt")

    # Load model
    task = kpu.load(model_addr)

    # Initialize UART
    fm.register(11, fm.fpioa.UART1_TX, force=True)
    fm.register(10, fm.fpioa.UART1_RX, force=True)
    uart = UART(UART.UART1, 115200, 8, 1, 0, timeout=1000, read_buf_len=4096)

    # Variables to track the last detected label and duration
    last_label = None
    label_start_time = 0

    try:
        while True:
            img = sensor.snapshot()
            t = time.ticks_ms()
            fmap = kpu.forward(task, img)
            t = time.ticks_ms() - t
            plist = fmap[:]
            pmax = max(plist)
            max_index = plist.index(pmax)
            current_label = labels[max_index].strip()

            # Display the detection result on the LCD
            img.draw_string(0, 0, "%.2f\n%s" % (pmax, current_label), scale=2, color=(255, 0, 0))
            img.draw_string(0, 200, "t:%dms" % (t), scale=2, color=(255, 0, 0))
            lcd.display(img)

            # Check if the detected label is a target label
            if current_label in TARGET_LABELS:
                # If the label is the same as the previous one, check the time
                if current_label == last_label:
                    elapsed_time = time.ticks_ms() - label_start_time
                    if elapsed_time > 1000:  # 3 seconds threshold
                        uart.write(current_label.encode())  # Send the label via UART
                        print("Sent over UART:", current_label)
                        label_start_time = time.ticks_ms()  # Reset timer to avoid repeated sending
                else:
                    # New label detected, start timer
                    last_label = current_label
                    label_start_time = time.ticks_ms()
            else:
                # Reset tracking if the label is not in TARGET_LABELS
                last_label = None

    except Exception as e:
        sys.print_exception(e)
    finally:
        # Cleanup
        kpu.deinit(task)
        uart.deinit()
        del uart
        gc.collect()

if __name__ == "__main__":
    try:
        with open("labels.txt") as f:
            labels = f.readlines()
        main(labels=labels, model_addr=0x300000, lcd_rotation=0, sensor_hmirror=False, sensor_vflip=False)
    except Exception as e:
        sys.print_exception(e)
    finally:
        gc.collect()
