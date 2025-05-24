package com.mattkc.vanilla;

import org.libsdl.app.SDLActivity;

public class VanillaActivity extends SDLActivity {
	@Override
	protected String[] getLibraries() {
		return new String[] {
			"SDL2",
			"SDL2_image",
			// "SDL2_mixer",
			// "SDL2_net",
			"SDL2_ttf",
			"vanilla-pi",
			"xml2",
			"main"
		};
	}
}
