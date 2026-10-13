package com.paldoyadam.game;

import org.libsdl.app.SDLActivity;

/** Android entry point only. Gameplay, input, UI and rendering are implemented in C. */
public final class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
