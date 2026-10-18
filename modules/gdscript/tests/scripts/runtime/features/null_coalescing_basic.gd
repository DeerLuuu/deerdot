extends Node

var nullable_text: String? = "x"
var nullable_number: int? = null
var value := 5

func test() -> void:
	print(null ?? 3)
	print(nullable_text ?? "fallback")
	print(nullable_number ?? 4)
	print(value ?? 0)
