begin_version
3
end_version
begin_metric
1
end_metric
6
begin_variable
var0
-1
6
Atom truck-at(t0, l1)
Atom truck-at(t0, l2)
Atom truck-at(t0, l3)
Atom truck-at(t0, l4)
Atom truck-at(t0, l5)
Atom truck-at(t0, l6)
end_variable
begin_variable
var1
-1
2
Atom visited(l1)
NegatedAtom visited(l1)
end_variable
begin_variable
var2
-1
2
Atom visited(l2)
NegatedAtom visited(l2)
end_variable
begin_variable
var3
-1
2
Atom visited(l4)
NegatedAtom visited(l4)
end_variable
begin_variable
var4
-1
2
Atom visited(l5)
NegatedAtom visited(l5)
end_variable
begin_variable
var5
-1
2
Atom visited(l6)
NegatedAtom visited(l6)
end_variable
1
begin_mutex_group
6
0 0
0 1
0 2
0 3
0 4
0 5
end_mutex_group
begin_state
2
1
1
1
1
1
end_state
begin_goal
5
1 0
2 0
3 0
4 0
5 0
end_goal
10
begin_operator
drive t0 l1 l3
0
1
0 0 0 2
2
end_operator
begin_operator
drive t0 l2 l3
0
1
0 0 1 2
1
end_operator
begin_operator
drive t0 l3 l1
0
2
0 0 2 0
0 1 -1 0
2
end_operator
begin_operator
drive t0 l3 l2
0
2
0 0 2 1
0 2 -1 0
1
end_operator
begin_operator
drive t0 l3 l4
0
2
0 0 2 3
0 3 -1 0
1
end_operator
begin_operator
drive t0 l3 l5
0
2
0 0 2 4
0 4 -1 0
0
end_operator
begin_operator
drive t0 l4 l3
0
1
0 0 3 2
1
end_operator
begin_operator
drive t0 l5 l3
0
1
0 0 4 2
0
end_operator
begin_operator
drive t0 l5 l6
0
2
0 0 4 5
0 5 -1 0
3
end_operator
begin_operator
drive t0 l6 l5
0
2
0 0 5 4
0 4 -1 0
3
end_operator
0
