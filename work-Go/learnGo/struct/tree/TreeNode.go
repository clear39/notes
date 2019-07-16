package main

import "fmt"

type TreeNode struct {
	Left,Right *TreeNode
	Value int
}

func (node TreeNode) print(){
	fmt.Print(node.Value)
}

func main()  {
	var a int
	fmt.Println(a)
	var root TreeNode
	root = TreeNode{Value:5,Left:nil,Right:nil}
	root.print()
	fmt.Println()

	
}