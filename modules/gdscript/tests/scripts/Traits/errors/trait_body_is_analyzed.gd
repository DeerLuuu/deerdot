# A trait's function body is analyzed where the trait is declared, even when
# nothing uses the trait: a broken trait must not wait for a consumer to be
# reported.

trait Broken:
	func oops() -> int:
		return "text"


func test():
	pass
