import cv2
from ultralytics import YOLO

MODEL_PATH = "D:\\ml\\runs\\detect\\train-6\\weights\\best.pt"

model = YOLO(MODEL_PATH)
cap = cv2.VideoCapture(1)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    results = model(frame, verbose=False)

    ic_box = None
    notch_box = None

    for box in results[0].boxes:
        cls = results[0].names[int(box.cls)]
        x1, y1, x2, y2 = map(int, box.xyxy[0])
        conf = float(box.conf)

        color = (0, 165, 255) if cls == "IC" else (0, 255, 0)
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        cv2.putText(frame, f"{cls} {conf:.2f}", (x1, y1 - 5),
            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

        if cls == "IC":
            ic_box = (x1, y1, x2, y2)
        elif cls == "notch":
            notch_box = (x1, y1, x2, y2)

    if ic_box and notch_box:
        ic_center_x = (ic_box[0] + ic_box[2]) / 2

        # Bottom-right corner consistently touches the actual notch
        notch_x = notch_box[2]

        orientation = notch_x < ic_center_x
        label = "CORRECT (notch LEFT)" if orientation else "FLIPPED (notch RIGHT)"
        color = (0, 255, 0) if orientation else (0, 0, 255)
        cv2.putText(frame, label, (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX, 1.2, color, 3)
    elif ic_box:
        cv2.putText(frame, "IC found, notch not detected", (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 255), 2)
    else:
        cv2.putText(frame, "No IC detected", (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 3)

    cv2.imshow("IC Orientation Detector", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
