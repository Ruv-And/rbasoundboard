package com.soundboard.service;

public class ProcessorBusyException extends Exception {
    public ProcessorBusyException(String message) {
        super(message);
    }
}
