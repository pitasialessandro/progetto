// librerie necessarie per IO di file
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
// Set
import java.util.Set;
import java.util.TreeSet;
// Map e Hashmap
import java.util.Map;
import java.util.HashMap;
// List, ArrayList, Collections
import java.util.List;
import java.util.ArrayList;
import java.util.Collections;



class CreaGrafo {
    public static void main(String[] args) {
        if (args.length != 2) {
            System.out.println("Uso: java CreaGrafo attori.tsv titoli.tsv");
            System.exit(1);
        }

        Map<Integer, Attore> attori = new HashMap<>();
        // leggi file name.basics.tsv e costruisci la mappa Map<Integer, Attore> attori
        try (BufferedReader br = new BufferedReader(new FileReader(args[0]))) {
            // scarta 1a linea
            String linea = br.readLine();
            while((linea = br.readLine()) != null) {
                String[] campi = linea.split("\t");

                if (campi.length < 5) continue; // safety check

                String birthYear = campi[2];
                String profession = campi[4];

                if (birthYear.equals("\\N")) continue; // skip if no birthyear
                if (!profession.contains("actor") && !profession.contains("actress")) continue; // skip if no actor/actress

                int codice = Integer.parseInt(campi[0].substring(2)); // int codice rimuovendo 'nm'
                String nome = campi[1];
                try {
                    int anno = Integer.parseInt(birthYear);
                    attori.put(codice, new Attore(codice, nome, anno));
                } catch (NumberFormatException e) {
                    continue;
                }

            }
        } catch (IOException e) {
            e.printStackTrace();
        }
        // FINE LETTURA FILE attori.tsv
        // br.close non serve perche try chiude automaticamente il reader

        // Sorting dei codici degli attori
        // Crea lista dei codici degli attori e la ordina per scrittura ordinata
        List<Integer> codici = new ArrayList<>(attori.keySet());
        Collections.sort(codici);

        // apre nome.txt in scrittura
        try (BufferedWriter bw = new BufferedWriter(new FileWriter("nomi.txt"))) {

            for (Integer codice : codici) {
                // lookup in hashmap
                Attore a = attori.get(codice);
                // Scrive ogni attore su una riga del file nomi.txt in formato TSV
                String riga = a.codice + "\t" + a.nome + "\t" + a.anno;

                bw.write(riga);
                bw.newLine();
            }

        } catch (IOException e) {
            e.printStackTrace();
        }

        // FINE SCRITTURA FILE nomi.txt
        // INIZIO LETTURA FILE title.tsv
        try (BufferedReader br = new BufferedReader(new FileReader(args[1]))) {
            Map<String, List<Integer>> castPerTitolo = new HashMap<>();
            String linea = br.readLine(); // scarta 1 linea
            while ((linea = br.readLine()) != null) {
                String[] campi = linea.split("\t");

                if (campi.length < 4) continue; // array safety

                String tconst = campi[0]; // codice film
                String nconst = campi[2]; // codice attore
                String categoria = campi[3];

                // Questa riga filtra solo gli attori che in quel film hanno effettivamente partecipato come attori, ma nei file corretti forniti non viene considerato
                // if (!categoria.equals("actor") && !categoria.equals("actress")) continue;

                int codiceAttore = Integer.parseInt(nconst.substring(2));

                // se l'attore e presente nella mappa attori, aggiungilo al cast del film
                if (attori.containsKey(codiceAttore)) {
                    castPerTitolo.putIfAbsent(tconst, new ArrayList<>());
                    // aggiungi attore alla lista del cast relativa a quel codice film
                    castPerTitolo.get(tconst).add(codiceAttore);
                }
            }
            // aggiorna i coprotagonisti
            for (List<Integer> cast : castPerTitolo.values()) {
                for (Integer a1 : cast) {
                    for (Integer a2 : cast) {
                        if (!a1.equals(a2)) {
                            attori.get(a1).coprotagonisti.add(a2);
                            // no check per i duplicati poiche TreeSet li evita e mantiene ordinati i valori
                        }
                    }
                }
            }
            // libero castPerTitolo poiche non utilizzo piu la Map
            castPerTitolo.clear();
        } catch (IOException e) {
            e.printStackTrace();
        }
        // debug output
        /*
        for (Attore a : attori.values()) {
            // usa StringBuilder per creare stringhe dinamicamente senza copiare piu volte String
            StringBuilder s = new StringBuilder();
            s.append(a.nome).append(" (").append(a.codice).append(") -> ");
            boolean first = true;
            for (Integer coprotagonistaID : a.coprotagonisti) {
                if (!first) s.append(", ");
                s.append(attori.get(coprotagonistaID).nome);
                first = false;
            }
            System.err.println(s.toString());
        }
        */
        // SCRITTURA IN grafo.txt
        try (BufferedWriter bw = new BufferedWriter(new FileWriter("grafo.txt"))) {
            // codici è già ordinato perche abbiamo applicato Collections.sort()
            for (Integer codice : codici) {
                Attore a = attori.get(codice);
                StringBuilder sb = new StringBuilder();
                sb.append(a.codice).append("\t");
                sb.append(a.coprotagonisti.size());
                for (Integer c : a.coprotagonisti) {
                    sb.append("\t").append(c);
                }
                bw.write(sb.toString());
                bw.newLine();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    static class Attore {
        int codice;
        String nome;
        int anno;
        Set<Integer> coprotagonisti;

        public Attore(int codice, String nome, int anno) {
            this.codice = codice;
            this.nome = nome;
            this.anno = anno;
            this.coprotagonisti = new TreeSet<>();
        }
    }
}