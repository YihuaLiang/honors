begin_version
3
end_version
begin_metric
1
end_metric
4
begin_variable
var0
-1
3
Atom truck-at(t0, l1)
Atom truck-at(t0, l2)
Atom truck-at(t0, l3)
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
Atom visited(l3)
NegatedAtom visited(l3)
end_variable
1
begin_mutex_group
3
0 0
0 1
0 2
end_mutex_group
begin_state
0
1
1
1
end_state
begin_goal
2
2 0
3 0
end_goal
4
begin_operator
drive t0 l1 l2
0
2
0 0 0 1
0 2 -1 0
1
end_operator
begin_operator
drive t0 l1 l3
0
2
0 0 0 2
0 3 -1 0
1
end_operator
begin_operator
drive t0 l2 l1
0
2
0 0 1 0
0 1 -1 0
1
end_operator
begin_operator
drive t0 l3 l1
0
2
0 0 2 0
0 1 -1 0
1
end_operator
0
