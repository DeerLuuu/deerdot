# A trait declared in an inner class is named with dots in a type test, and is
# answered for values whose static type does not mention the trait.

class Holder:
	trait Inner:
		var y := 2


class UsesHolder extends Node:
	uses Holder.Inner


class Single extends Node:
	pass


func test():
	var u: UsesHolder = UsesHolder.new()
	print(u is Holder.Inner)

	var n: Node = UsesHolder.new()
	print(n is Holder.Inner)

	var s: Node = Single.new()
	print(s is Holder.Inner)

	u.y = 5
	print(u.y)

	u.free()
	n.free()
	s.free()
