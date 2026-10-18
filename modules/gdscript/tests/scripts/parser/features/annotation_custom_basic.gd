extends Node

@Author("deer")
@Author("second")
class Inner:
	@warning_ignore("unused_signal")
	@Mark
	signal ping
	@Tag
	var value := 1
	@Route("/m", method = "POST")
	func method() -> void:
		pass

func test() -> void:
	print("ok")
