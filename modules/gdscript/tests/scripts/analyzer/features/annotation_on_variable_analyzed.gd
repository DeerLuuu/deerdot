extends Node

@tag(1)
var a := 1

@tag("x", named = 2)
var b := 2

func test() -> void:
	var script: GDScript = get_script()
	print(script.has_annotation(&"a", &"tag"))
	print(script.get_annotation(&"a", &"tag"))
	print(script.get_annotation(&"b", &"tag"))
