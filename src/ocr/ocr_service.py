import cv2
import easyocr
import socket
import threading
import time
import numpy as np

# --- Config ---
VALID_ICS = {'7404', '7408', '7432'}
HOST = '0.0.0.0'
PORT = 5000
CAM_INDEX = 0
PROCESS_EVERY = 15   # frames between OCR calls (OCR is slow ~200ms)
CONF_THRESHOLD = 0.5


reader = None  # lazy-init in background


def normalize_ocr(text: str) -> str:
    """Fix common OCR confusions for 74xx series labels."""
    return (
        text.strip()
        .replace(' ', '')
        .upper()
        .replace('O', '0')
        .replace('I', '1')
        .replace('L', '1')
        .replace('S', '5')
        .replace('B', '8')
    )


def bbox_is_upright(bbox) -> bool:
    """
    bbox: [[x1,y1],[x2,y2],[x3,y3],[x4,y4]] clockwise from top-left (EasyOCR format).
    Returns True when text flows left→right (normal reading orientation).
    For upside-down text the tl→tr vector points leftward (negative x).
    """
    tl = np.array(bbox[0], dtype=float)
    tr = np.array(bbox[1], dtype=float)
    right_vec = tr - tl
    return float(right_vec[0]) > 0


def detect(frame):
    """Returns (ic_id: str, aligned: bool). ic_id is 'none' when nothing found."""
    if reader is None:
        return 'none', False
    results = reader.readtext(frame, detail=1)

    best_ic = 'none'
    best_aligned = False
    best_conf = 0.0

    for (bbox, text, conf) in results:
        clean = normalize_ocr(text)
        if clean in VALID_ICS and conf > CONF_THRESHOLD and conf > best_conf:
            best_conf = conf
            best_ic = clean
            best_aligned = bbox_is_upright(bbox)

    return best_ic, best_aligned


# --- TCP server ---

def client_handler(conn, state: dict):
    last_sent = None
    try:
        while True:
            msg = f"{state['ic']},{int(state['aligned'])}\n"
            if msg != last_sent:
                conn.sendall(msg.encode('ascii'))
                last_sent = msg
            time.sleep(0.05)
    except (BrokenPipeError, ConnectionResetError, OSError):
        pass
    finally:
        conn.close()


def tcp_server(state: dict):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((HOST, PORT))
        srv.listen(5)
        print(f"[TCP] Listening on {HOST}:{PORT}")
        while True:
            conn, addr = srv.accept()
            print(f"[TCP] Client connected: {addr}")
            threading.Thread(
                target=client_handler, args=(conn, state), daemon=True
            ).start()


# --- Main loop ---

def _init_reader():
    global reader
    print("[OCR] Loading EasyOCR model...")
    reader = easyocr.Reader(['en'], gpu=True)
    print("[OCR] Model ready.")


def main():
    state = {'ic': 'none', 'aligned': False}

    threading.Thread(target=tcp_server, args=(state,), daemon=True).start()
    threading.Thread(target=_init_reader, daemon=True).start()

    print("[CAM] Opening camera...")
    cap = cv2.VideoCapture(CAM_INDEX, cv2.CAP_DSHOW)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    if not cap.isOpened():
        raise RuntimeError(f"Cannot open camera index {CAM_INDEX}")

    frame_n = 0
    print("[CAM] Running. Press Q to quit.")

    while True:
        ret, frame = cap.read()
        if not ret:
            print("[CAM] Frame grab failed.")
            break

        frame_n += 1
        if frame_n % PROCESS_EVERY == 0:
            ic, aligned = detect(frame)
            state['ic'] = ic
            state['aligned'] = aligned

        # Overlay
        label = "OCR loading..." if reader is None else f"IC: {state['ic']}  aligned={state['aligned']}"
        color = (0, 220, 255) if reader is None else ((0, 220, 0) if state['aligned'] else (0, 60, 220))
        cv2.putText(frame, label, (12, 44),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 0), 4)
        cv2.putText(frame, label, (12, 44),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, color, 2)
        cv2.imshow('IC OCR Service', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
