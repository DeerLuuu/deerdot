# `is Trait` asks about the actual object, not about the operand's static type:
# a base class, `Variant` or a null value does not decide the answer.

trait T:
	var x := 1


class Uses extends Node:
	uses T


class Base extends Node:
	pass


class Derived extends Base:
	uses T


func test():
	# A static type that is the using class itself is answered while analyzing.
	var u: Uses = Uses.new()
	print(u is T)

	# The static type is a base class, the value took the trait.
	var n: Node = Uses.new()
	print(n is T)

	# The trait comes from the derived class, the static type is the base one.
	var b: Base = Derived.new()
	print(b is T)

	# The same object seen through a Variant.
	var v: Variant = b
	print(v is T)

	# The value did not take the trait.
	var plain: Node = Node.new()
	print(plain is T)

	# A null value cannot be of the trait.
	var nothing = null
	print(nothing is T)

	u.free()
	n.free()
	b.free()
	plain.free()
