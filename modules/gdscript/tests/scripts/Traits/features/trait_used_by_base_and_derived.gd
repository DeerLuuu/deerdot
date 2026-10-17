# A base class and its derived class both use the same trait. The derived class
# does not copy the trait's variable a second time: the base class already
# provides it. This used to crash when the derived class was instantiated.

trait T:
	var x := 1


class A extends Node:
	uses T


class B extends A:
	uses T


func test():
	var a := A.new()
	assert(a.x == 1)
	a.free()

	var b := B.new()
	assert(b.x == 1)
	b.free()
