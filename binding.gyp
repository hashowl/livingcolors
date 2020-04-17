{
  "targets": [
    {
      "target_name": "livingcolors",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [
	    "./src/index.cpp",
        "./src/livingcolors.cpp",
		"./src/cc2500.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "link_settings": {
        "libraries": [ "-lwiringPi" ],
      },
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
    }
  ]
}