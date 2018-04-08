(define (problem travel-test)
(:domain travel-strips)

(:objects 
l1 l2 l3 l4 l5 l6 - city
t0 - truck
)

(:init
(next l1 l3)(next l2 l3)(next l4 l3)(next l5 l3)(next l5 l6) 
(next l3 l1)(next l3 l2)(next l3 l4)(next l3 l5)(next l6 l5)

(truck-at t0 l3)
(visited l3)

(= (drive l1 l3) 2) (= (drive l2 l3) 1) (= (drive l4 l3) 1) (= (drive l3 l5) 0) (= (drive l5 l6) 3) 
(= (total-cost) 0)

)

(:goal
(and
(visited l1)
(visited l2)
(visited l3)
(visited l4)
(visited l5)
(visited l6)
))

(:metric minimize (total-cost))

)
