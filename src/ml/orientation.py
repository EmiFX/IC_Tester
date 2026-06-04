from ultralytics import YOLO
import cv2
import os

MODEL_PATH = "D:\\instrumentation_final\\runs\\detect\\train-6\\weights\\best.pt"
TEST_IMAGES_DIR = "D:\\instrumentation_final\\dataset_clean\\test\\images"
OUTPUT_DIR = "D:\\instrumentation_final\\output"

os.makedirs(OUTPUT_DIR, exist_ok=True)

model = YOLO(MODEL_PATH)

for image_name in os.listdir(TEST_IMAGES_DIR):
    if not image_name.lower().endswith((".jpg", ".jpeg", ".png")):
        continue

    image_path = os.path.join(TEST_IMAGES_DIR, image_name)
    frame = cv2.imread(image_path)

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

        # Bottom-right corner of notch box consistently touches the actual notch
        notch_x = notch_box[2]

        orientation = notch_x < ic_center_x
        label = "CORRECT (notch LEFT)" if orientation else "FLIPPED (notch RIGHT)"
        color = (0, 255, 0) if orientation else (0, 0, 255)
        print(f"{image_name}: {label}")
    elif ic_box:
        label = "IC found, notch not detected"
        color = (0, 255, 255)
        print(f"{image_name}: notch not detected")
    else:
        label = "No IC detected"
        color = (0, 0, 255)
        print(f"{image_name}: no IC detected")

    cv2.putText(frame, label, (20, 40),
        cv2.FONT_HERSHEY_SIMPLEX, 1.2, color, 3)

    output_path = os.path.join(OUTPUT_DIR, image_name)
    cv2.imwrite(output_path, frame)

print(f"\nDone. Results saved to {OUTPUT_DIR}")
