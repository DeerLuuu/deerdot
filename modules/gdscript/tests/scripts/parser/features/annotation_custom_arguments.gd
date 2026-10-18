extends Node

@Route("/save", method = "POST", priority = 3)
func save() -> void:
	pass

@Tag(1, "two", 3.0, true)
func tagged() -> void:
	pass

func test() -> void:
	print("ok")
