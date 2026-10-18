extends Node

var integer: int? = null
var text: String? = null


func test() -> void:
	print(integer, text)
	integer = 5
	text = "hello"
	print(integer, text)
	integer = null
	print(integer)
