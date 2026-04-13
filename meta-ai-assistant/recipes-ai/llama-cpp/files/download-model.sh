#!/bin/sh
MODEL_DIR=/opt/llama.cpp/models
MODEL=$MODEL_DIR/tinyllama.gguf
MODEL_URL="https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"

if [ ! -f "$MODEL" ]; then
    echo "Downloading TinyLlama model..."
    mkdir -p $MODEL_DIR
    wget --no-check-certificate "$MODEL_URL" -O "$MODEL"
    echo "Model downloaded."
fi
