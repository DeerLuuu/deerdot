extends Node

var node_value: Node? = null
var text_value: String? = null
var number_value: int? = 5
var list_value: Array[int]? = null
var legacy_value: Node = null


func maybe_node() -> Node?:
	return null


func maybe_text(value: int?) -> String?:
	if value == null:
		return null
	return str(value)


func test() -> void:
	var local: Node? = maybe_node()
	print(node_value, text_value, number_value, list_value, local)
	print(maybe_text(null), maybe_text(3))
