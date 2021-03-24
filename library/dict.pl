:- module(dict, [get/4, get/3, set/4, del/3, lst/2]).

get([], _, D, D) :- !.
get([N:V|_], N, V, _) :- !.
get([_|T], N, V, D) :-
	get(T, N, V, D).

get([], _, _) :- !,
	fail.
get([N:V|_], N, V) :- !.
get([_|T], N, V) :-
	get(T, N, V).

set([], N, V, [N:V]) :- !.
set(D, N, V, D2) :-
	del(D, N, D3),
	D2=[N:V|D3].

del([], _, []) :- !.
del([N:_|T], N, T) :- !.
del([H|T], N, [H|D]) :-
	del(T, N, D).

lst0([], L, L) :- !.
lst0([_:V|T], L1, L) :-
	lst0(T, [V|L1], L).

lst(D, L) :-
	lst0(D, [], L).
