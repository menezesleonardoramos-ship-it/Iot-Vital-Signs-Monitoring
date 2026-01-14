print("Servidor iniciando...")


from flask import Flask, request, jsonify, render_template
from flask_socketio import SocketIO
import sqlite3
from datetime import datetime

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")



def db():
    return sqlite3.connect("dados.db")

# Inicializa o banco de dados
with db() as conn:
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS dados (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            temperatura REAL,
            bpm INTEGER,
            spo2 INTEGER,
            timestamp TEXT
        )
    """)
    conn.commit()

@app.route("/")
def realtime():
    return render_template("realtime.html")

@app.route("/historico")
def historico():
    conn = db()
    c = conn.cursor()
    c.execute("SELECT * FROM dados ORDER BY id DESC LIMIT 50")
    dados = c.fetchall()
    conn.close()
    return render_template("historico.html", dados=dados)

@app.route("/api/dados", methods=["POST"])
def receber():
    d = request.json
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    conn = db()
    c = conn.cursor()
    c.execute(
        "INSERT INTO dados (temperatura, bpm, spo2, timestamp) VALUES (?, ?, ?, ?)",
        (d["temp"], d["bpm"], d["spo2"], now)
    )
    conn.commit()
    conn.close()

    socketio.emit("novo_dado", d)
    return jsonify({"status": "ok"})

if __name__ == "__main__":
    socketio.run(app, host="0.0.0.0", port=5000, debug=True)

