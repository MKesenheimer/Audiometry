#!/usr/bin/env python
import matplotlib.pyplot as plt

#import pygame, pygame.sndarray
#import numpy
#import scipy.signal
#from time import sleep
#sample_rate = 48000
#pygame.mixer.pre_init(sample_rate, -16, 1, 1024)
#pygame.init()
#def square_wave(hz, peak, duty_cycle=.5, n_samples=sample_rate):
#    t = numpy.linspace(0, 1, 500 * 440/hz, endpoint=False)
#    wave = scipy.signal.square(2 * numpy.pi * 5 * t, duty=duty_cycle)
#    wave = numpy.resize(wave, (n_samples,))
#    return (peak / 2 * wave.astype(numpy.int16))
#def audio_freq(freq = 800):
#    global sound
#    sample_wave = square_wave(freq, 4096)
#    sound = pygame.sndarray.make_sound(sample_wave)
#audio_freq()
#sound.play(-1)
#sleep(0.5)
#sound.stop()

#import pysine
#pysine.sine(frequency=440.0, duration=1.0) 

from tones import SINE_WAVE, SAWTOOTH_WAVE
from tones.mixer import Mixer

mixer = Mixer(44100, 0.5)
mixer.create_track(0, SAWTOOTH_WAVE, vibrato_frequency=7.0, vibrato_variance=30.0, attack=0.01, decay=0.1)
mixer.create_track(1, SINE_WAVE, attack=0.01, decay=0.1)
mixer.add_note(0, note='c#', octave=5, duration=1.0, endnote='f#')
mixer.write_wav('tones.wav')
samples = mixer.mix()


plt.plot([1, 2, 3, 4])
plt.ylabel('some numbers')
plt.show()
