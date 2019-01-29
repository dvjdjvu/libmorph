{
  "targets": [
    {
      "target_name": "morph",
      "sources": [ "src/morph.cpp" ],
      "ldflags": ["-lmorph"],
      "libraries": ["-lmorph"],
      "cflags": ["-lmorph"],
      "link_settings": {
	"libraries": ["-lmorph"]
      }
    }
  ]
}
