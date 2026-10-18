extends Node

var node: Node? = Node.new()


func test() -> void:
	print(node.name)
	node.free()
