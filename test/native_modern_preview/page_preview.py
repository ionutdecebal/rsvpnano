"""A local window for the production Page renderer with synthetic reader data."""

import subprocess
import tkinter as tk
from tkinter import ttk


WIDTH = 640
HEIGHT = 172
PPM_HEADER = b"P6\n640 172\n255\n"


def show(binary):
    root = tk.Tk()
    root.title("RSVP Nano — Page preview")
    root.resizable(False, False)
    frame = ttk.Frame(root, padding=16)
    frame.pack()
    display = ttk.Label(frame)
    display.pack()
    controls = ttk.Frame(frame, padding=(0, 12, 0, 0))
    controls.pack(fill="x")

    with subprocess.Popen(
        [str(binary), "--page-interactive"], stdin=subprocess.PIPE, stdout=subprocess.PIPE
    ) as process:
        def send(command, x=0, y=0):
            process.stdin.write(f"{command} {x} {y}\n".encode())
            process.stdin.flush()
            header = process.stdout.read(len(PPM_HEADER))
            pixels = process.stdout.read(WIDTH * HEIGHT * 3)
            if header != PPM_HEADER or len(pixels) != WIDTH * HEIGHT * 3:
                root.destroy()
                raise RuntimeError("The native Page renderer did not produce a complete frame")
            display.image = tk.PhotoImage(data=header + pixels, format="PPM")
            display.configure(image=display.image)

        def touch(command, event):
            send(command, min(max(event.x, 0), WIDTH - 1), min(max(event.y, 0), HEIGHT - 1))

        ttk.Button(controls, text="Middle button", command=lambda: send(4)).pack(side="left")
        ttk.Button(controls, text="Page settings", command=lambda: send(5)).pack(side="left", padx=8)
        display.bind("<Button-1>", lambda event: touch(1, event))
        display.bind("<B1-Motion>", lambda event: touch(2, event))
        display.bind("<ButtonRelease-1>", lambda event: touch(3, event))
        root.bind("<Up>", lambda event: (send(1, 608, 70), send(3, 608, 70)))
        root.bind("<Down>", lambda event: (send(1, 608, 120), send(3, 608, 120)))
        try:
            send(0)
            root.mainloop()
        finally:
            process.stdin.close()
