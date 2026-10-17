## 护甲：块式声明示例（`trait Armored:` + 缩进块）。
trait Armored:
	var armor := 10

	func absorb(amount: int) -> int:
		var taken := mini(armor, amount)
		armor -= taken
		return amount - taken
