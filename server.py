from fastapi import FastAPI, File, UploadFile
from fastapi.responses import FileResponse
from PIL import Image
import pytesseract
from gtts import gTTS
import uuid
import os

app = FastAPI()

@app.post("/ocr-to-speech/")
async def ocr_to_speech(image: UploadFile = File(...)):
    # Save image
    image_path = f"temp_{uuid.uuid4().hex}.jpg"
    with open(image_path, "wb") as f:
        f.write(await image.read())

    # OCR
    text = pytesseract.image_to_string(Image.open(image_path))
    if not text.strip():
        return {"error": "No text found in image"}

    # Convert text to speech
    tts = gTTS(text=text, lang='en')
    mp3_path = f"{uuid.uuid4().hex}.mp3"
    tts.save(mp3_path)

    # Clean up image
    os.remove(image_path)

    # Return MP3 file
    return FileResponse(mp3_path, media_type="audio/mpeg", filename="speech.mp3")
