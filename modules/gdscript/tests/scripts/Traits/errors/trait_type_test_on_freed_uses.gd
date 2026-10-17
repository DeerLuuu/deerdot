# `is SomeTrait` asks about the value, like `is SomeClass`: a freed instance is
# an error for both, not a silent `true` from the static type.

trait T:
	var x := 1


class Uses extends Node:
	uses T


func test_trait():
	var u: Uses = Uses.new()
	u.free()
	print(u is T)


func test_class():
	var u: Uses = Uses.new()
	u.free()
	print(u is Uses)


func test():
	test_trait()
	test_class()
