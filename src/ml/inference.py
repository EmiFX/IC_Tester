from sahi import AutoDetectionModel
from sahi.predict import get_sliced_prediction
import os

# Paths
MODEL_PATH = "D:\\instrumentation_final\\runs\\detect\\train-6\\weights\\best.pt"
TEST_IMAGES_DIR = "D:\\instrumentation_final\\dataset_clean\\test\\images"
OUTPUT_DIR = "D:\\instrumentation_final\\output"

os.makedirs(OUTPUT_DIR, exist_ok=True)

# Load model
model = AutoDetectionModel.from_pretrained(
    model_type="ultralytics",
    model_path=MODEL_PATH,
    confidence_threshold=0.4,
    device="cpu"
)

# Run inference on every image in test folder
for image_name in os.listdir(TEST_IMAGES_DIR):
    if not image_name.lower().endswith((".jpg", ".jpeg", ".png")):
        continue

    image_path = os.path.join(TEST_IMAGES_DIR, image_name)
    print(f"Processing: {image_name}")

    result = get_sliced_prediction(
        image_path,
        model,
        slice_height=256,
        slice_width=256,
        overlap_height_ratio=0.2,
        overlap_width_ratio=0.2
    )

    # Print detections for this image
    for detection in result.object_prediction_list:
        print(f"  {detection.category.name} - confidence: {detection.score.value:.2f}")

    # Save annotated image
    result.export_visuals(
        export_dir=OUTPUT_DIR,
        file_name=image_name.split(".")[0]
    )

print(f"\nDone. Results saved to {OUTPUT_DIR}")
