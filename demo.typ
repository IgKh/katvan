// katvan: font-size 14; font DejaVu Sans Mono;
#set text(lang: "he", font: "David CLM")
#set page(width: 10cm, height: auto)
#set heading(numbering: "1.")

= סדרת פיבונאצ'י
סדרת פיבונאצ'י מוגדרת באמצעות נוסחת הנסיגה ⁦$F_n = F_(n-1) + F_(n+2)$⁩. ניתן גם לייצג אותה באמצעות _הנוסחא הסגורה_:

$ F_n = round(1 / sqrt(5) phi.alt^n), quad phi.alt = (1 + sqrt(5)) / 2 $

#let count = 8
#let nums = range(1, count + 1)
#let fib(n) = (
    if n <= 2 { 1 }
    else { fib(n - 1) + fib(n - 2) }
)

⁦#count⁩ המספרים הראשונים בסדרה הם:

#text(dir: ltr,
    align(center,
        table(
            columns: count,
            ..nums.map(n => $F_#n$),
            ..nums.map(n => str(fib(n))),
        )
    )
)

