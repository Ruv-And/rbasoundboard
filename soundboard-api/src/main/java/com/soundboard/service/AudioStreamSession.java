package com.soundboard.service;

import soundboard.AudioProcessorOuterClass;

import java.util.Iterator;

public class AudioStreamSession {
    private final Iterator<AudioProcessorOuterClass.AudioChunk> iterator;
    private final byte[] firstChunk;

    public AudioStreamSession(Iterator<AudioProcessorOuterClass.AudioChunk> iterator, byte[] firstChunk) {
        this.iterator = iterator;
        this.firstChunk = firstChunk;
    }

    public Iterator<AudioProcessorOuterClass.AudioChunk> getIterator() {
        return iterator;
    }

    public byte[] getFirstChunk() {
        return firstChunk;
    }
}
