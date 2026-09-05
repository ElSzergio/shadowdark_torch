# Shadowdark Torch — M5Stack CoreS3

Contador de antorcha para **Shadowdark RPG**. En Shadowdark una antorcha dura
**una hora de tiempo real**: este cacharro la lleva por ti en la mesa, sin
cronómetros de móvil ni cuentas mentales.

```
      /\        antorcha en pixel art, la llama se anima a 4 fps
     /  \
    ( ** )
      ||
   [========------------]   barra = 200 fragmentos, 85% del ancho
```

## Cómo se juega

| Acción | Gesto |
|---|---|
| **Encender** | Sacude el CoreS3 con ganas (varios meneos seguidos) |
| **Apagar** | Sopla fuerte sobre los micrófonos del lateral |
| **Se consume** | Sola, a los 60 minutos |
| **Pantalla en negro** | Un toque en la pantalla; otro toque la devuelve |

La barra **no muestra minutos**, solo cuánta antorcha queda: 200 fragmentos,
uno cada 18 segundos. En el último 10% la llama se encoge y se apaga a
ratos; en el último 5% la barra late. Cuando llega a cero la antorcha se
apaga y hay que volver a sacudir para prender una nueva.

## Detalles de implementación

**Encendido por sacudida.** No basta un pico de aceleración: se exigen
`SHAKE_PEAKS_NEEDED` (3) picos de más de `SHAKE_PEAK_G` (1,6 g sobre el
reposo) separados al menos 70 ms, todos dentro de una ventana de 1,5 s. Un
golpe a la mesa o alguien que coge el aparato produce un único pico y no
enciende nada.

**Apagado soplando.** Un soplido tiene que cumplir **tres** condiciones a la
vez, y la que de verdad importa es la primera:

1. **Grave.** Soplar es turbulencia de aire: casi toda su energía está por
   debajo de ~120 Hz. La voz vive muy por encima. Se filtra la señal con dos
   polos a 120 Hz y se exige que lo grave pese más de `BLOW_LF_RATIO_MIN`
   (0,45) sobre el total. **Éste es el filtro que de verdad separa soplar de
   hablar**; los otros dos son de apoyo.
2. **Fuerte.** Por encima del 5,5% del fondo de escala y de 6× el ruido
   ambiente medido en vivo (así vale igual en mesa callada que en un bar).
3. **Sostenido.** 320 ms seguidos, con histéresis: una vez empezado el
   soplido los umbrales bajan al 70% hasta que para, porque un soplido real
   da bandazos. Descarta plosivas, palmadas y golpes en la mesa, que son
   graves y fuertes pero duran 100 ms.

Medido **en un CoreS3 real**, con el medidor de pantalla:

| Sonido | Nivel | Peso de graves | ¿Apaga? |
|---|---|---|---|
| Soplar fuerte | **55% FS** | **0,60** | sí |
| Hablar cerca, como en partida | bajo | muy por debajo de 0,45 | no |

Dos avisos que salieron de calibrar esto con el aparato delante:

- **El volumen no distingue voz de soplido.** Gritar supera cualquier umbral
  de nivel razonable. La primera versión miraba solo volumen y duración, y
  hablar cerca de la mesa apagaba la antorcha a los 15 minutos.
- **Los números del micro real no se parecen a los de la simulación.** El
  micro del CoreS3 es unas 4 veces más sensible de lo que suponía el modelo y
  además recorta los graves: el soplido da 0,60 de peso de graves donde la
  simulación predecía 1,4. Si tocas umbrales, mídelos con el medidor, no con
  `blowsim.py` a secas.

**Pantalla en negro.** Además de pintarla de negro apaga la
retroiluminación, para no iluminar la mesa en una partida a oscuras. En ese
modo no se dibuja nada ni se leen los sensores: solo responde al siguiente
toque.

**Sin parpadeos.** Cada cuadro se compone entero en un lienzo en PSRAM y se
vuelca de golpe.

## Decisiones que tomé por ti

Están todas en [`src/config.h`](src/config.h) como constantes, cambiar
cualquiera es una línea:

| Decisión | Valor | Alternativa |
|---|---|---|
| Con la pantalla en negro **el tiempo sigue corriendo** | `BLACKOUT_PAUSES_TIMER = false` | `true` congela el contador |
| La pantalla en negro **apaga la retroiluminación** | `BLACKOUT_TURNS_OFF_BACKLIGHT = true` | `false` solo pinta negro |
| Soplar **apaga la antorcha del todo**: la siguiente sacudida enciende una nueva de 60 min | `RESUME_AFTER_BLOWOUT = false` | `true` guarda el tiempo restante y lo reanuda al reencender |
| Los rótulos están en inglés (`SHAKE TO LIGHT`, `BURNED OUT`) | — | `drawMessage(...)` en `src/main.cpp` |

La segunda fila importa si en tu mesa apagáis la antorcha para *guardarla*:
con `RESUME_AFTER_BLOWOUT = true` soplar deja de gastar antorcha.

## Compilar y flashear

```bash
pio run -t upload && pio device monitor
```

## Si se apaga sola: cómo averiguar por qué

Con `SHOW_OUT_DEBUG = true` (viene activado) la pantalla te dice **qué pasó**,
y son tres cosas distintas:

| Lo que ves | Qué pasó |
|---|---|
| `SNUFFED` + `OUT AT 15:23 - SNUFFED` | La dio por soplada. Falso positivo del micrófono |
| `BURNED OUT` + `OUT AT 60:00 - BURNED` | Se agotó de verdad. Si el número no es ~60:00, es fallo del contador |
| `SHADOWDARK / SHAKE TO LIGHT` + `BOOT: ...` | **El aparato se reinició**. El rótulo dice si fue caída de tensión (`BROWNOUT`), cuelgue (`CRASH`, `WATCHDOG`) o desenchufe (`POWER ON`) |

Cuando ya confíes en ella, pon `SHOW_OUT_DEBUG = false` en `src/config.h` y
desaparecen los rótulos.

## Herramientas

Las dos leen `src/` directamente, así que siempre reflejan lo que hay
compilado:

```bash
python3 tools/preview.py salida.png   # cómo se verá la pantalla, sin flashear
python3 tools/blowsim.py              # enfrenta el detector a voz, palmadas y soplidos
```

Si tocas cualquier umbral del micrófono, `blowsim.py` te dice en un segundo si
lo has roto.

## Calibrar el soplido

Con `SHOW_MIC_METER = true` (viene activado) la antorcha **encendida** escribe
bajo la barra lo que oye el micrófono, con retención de picos de 5 segundos
para que puedas soplar primero y leer después:

```
LVL 2.4>18.2%   LF 0.31>1.48   B320
 |    |          |    |          `- ms de soplido acumulados; si sube, pasa
 |    |          |    `----------- pico de graves de los últimos 5 s
 |    |          `---------------- graves ahora  (umbral: BLOW_LF_RATIO_MIN)
 |    `-------------------------- pico de nivel de los últimos 5 s
 `------------------------------- nivel ahora, % del fondo de escala
```

Enciende la antorcha, sopla fuerte y lee los dos picos:

| Lo que veas al soplar | Qué significa |
|---|---|
| `MIC OFF` | El micrófono no arrancó. Ningún umbral lo va a arreglar |
| `LVL` no pasa de ~1% | El micrófono capta muy flojo: baja `BLOW_MIN_LEVEL_PCT` |
| `LF` no llega al umbral | Tu micro filtra los graves: baja `BLOW_LF_RATIO_MIN`, o sube `MIC_LP_ALPHA` para medir hasta más arriba |
| `B` sube hasta 600 | Funciona: se apaga |

Después habla cerca y mira `LF`: la distancia entre ese número y el que da al
soplar es todo el margen que tienes. Pon `BLOW_LF_RATIO_MIN` a la mitad de
camino, y `SHOW_MIC_METER = false` cuando esté ajustado.

## Calibrar por el puerto serie

El nivel del micrófono depende de tu unidad y de la sala. Si te cuesta
apagarla o se apaga sola, pon `#define MIC_DEBUG 1` en
[`src/main.cpp`](src/main.cpp), abre el monitor serie y sopla:

```
[mic] nivel=412 (1.3% FS)  graves=0.19  ruido=380  umbral=1638  soplido=0 ms
[mic] nivel=4980 (15.2% FS) graves=1.44 ruido=381  umbral=1638  soplido=208 ms  <- soplando
```

Habla cerca y mira la columna `graves`: mientras se quede por debajo de 0,60
no hay forma de que la voz apague la antorcha. Si tu voz llegara más arriba,
sube `BLOW_LF_RATIO_MIN`; si te cuesta apagarla soplando, baja
`BLOW_MIN_LEVEL_PCT`. Lo mismo con `SHAKE_PEAK_G` si la
sacudida te resulta dura o blanda.

## Estructura

```
src/config.h      todos los parámetros ajustables
src/torch_art.h   el pixel art (4 fotogramas de llama + cuerpo + apagada)
src/main.cpp      máquina de estados, sensores y dibujo
tools/preview.py  previsualiza la pantalla en el ordenador
tools/blowsim.py  valida el detector de soplido contra sonidos sintéticos
```

El arte son cadenas de texto, un carácter por píxel: se edita a mano sin
herramientas. La rejilla es de 16 columnas y cada píxel se dibuja a
`ART_SCALE` (7) píxeles de pantalla.
