#const unitCap = 2.
#const interUnitCap = 2.

{ zone2sensor(Z,S) : zone(Z), sensor(S) }.
#instance zone2sensor/2.

zone(1..10).
sensor(1..10).
unit(1..10).

1 { unit2zone(U,Z) : unit(U) } 1 :- zone(Z).
1 { unit2sensor(U,S) : sensor(U) } 1 :- sensor(S).

:- unit(U), unitCap + 1 { unit2zone(U,Z) : zone(Z) }.
:- unit(U), unitCap + 1 { unit2sensor(U,S) : sensor(S) }.

partnerunits(U,P) :- unit2zone(U,Z), zone2sensor(Z,S), unit2sensor(P,S), U != P.
partnerunits(U,P) :- partnerunits(P,U), unit(U), unit(P).

:- unit(U), interUnitCap + 1 { partnerunits(U,P) : unit(P) }.
